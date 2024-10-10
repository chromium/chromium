// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder.h"

#include <memory>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/gpu/windows/d3d11_av1_accelerator.h"
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/gpu/windows/d3d11_h265_accelerator.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
#include "media/gpu/windows/d3d11_video_frame_mailbox_release_helper.h"
#include "media/gpu/windows/d3d12_video_decoder_wrapper.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "media/media_buildflags.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_switches.h"

namespace media {

namespace {

// Holder class, so that we don't keep creating CommandBufferHelpers every time
// somebody calls a callback.  We can't actually create it until we're on the
// right thread.
struct CommandBufferHelperHolder
    : base::RefCountedDeleteOnSequence<CommandBufferHelperHolder> {
  CommandBufferHelperHolder(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : base::RefCountedDeleteOnSequence<CommandBufferHelperHolder>(
            std::move(task_runner)) {}

  CommandBufferHelperHolder(const CommandBufferHelperHolder&) = delete;
  CommandBufferHelperHolder& operator=(const CommandBufferHelperHolder&) =
      delete;

  scoped_refptr<CommandBufferHelper> helper;

 private:
  ~CommandBufferHelperHolder() = default;
  friend class base::RefCountedDeleteOnSequence<CommandBufferHelperHolder>;
  friend class base::DeleteHelper<CommandBufferHelperHolder>;
};

scoped_refptr<CommandBufferHelper> CreateCommandBufferHelper(
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb,
    scoped_refptr<CommandBufferHelperHolder> holder) {
  gpu::CommandBufferStub* stub = get_stub_cb.Run();
  if (!stub)
    return nullptr;

  DCHECK(holder);
  if (!holder->helper)
    holder->helper = CommandBufferHelper::Create(stub);

  return holder->helper;
}

}  // namespace

std::unique_ptr<VideoDecoder> D3D11VideoDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb,
    D3D11VideoDecoder::GetD3DDeviceCB get_d3d_device_cb,
    SupportedConfigs supported_configs) {
  // Note that the output callback will hop to our thread, post the video
  // frame, and along with a callback that will hop back to the impl thread
  // when it's released.
  // Note that we WrapUnique<VideoDecoder> rather than D3D11VideoDecoder to make
  // this castable; the deleters have to match.
  auto get_helper_cb = base::BindRepeating(
      CreateCommandBufferHelper, std::move(get_stub_cb),
      base::MakeRefCounted<CommandBufferHelperHolder>(gpu_task_runner));
  return base::WrapUnique<VideoDecoder>(new D3D11VideoDecoder(
      gpu_task_runner, std::move(media_log), gpu_preferences, gpu_workarounds,
      get_helper_cb, std::move(get_d3d_device_cb),
      std::move(supported_configs)));
}

D3D11VideoDecoder::D3D11VideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb,
    GetD3DDeviceCB get_d3d_device_cb,
    SupportedConfigs supported_configs)
    : media_log_(std::move(media_log)),
      mailbox_release_helper_(
          base::MakeRefCounted<D3D11VideoFrameMailboxReleaseHelper>(
              media_log_->Clone(),
              get_helper_cb)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      decoder_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      get_d3d_device_cb_(std::move(get_d3d_device_cb)),
      get_helper_cb_(std::move(get_helper_cb)),
      supported_configs_(std::move(supported_configs)),
      use_shared_handle_(
          base::FeatureList::IsEnabled(kD3D12VideoDecoder) ||
          base::FeatureList::IsEnabled(kD3D11VideoDecoderUseSharedHandle)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_log_);
}

D3D11VideoDecoder::~D3D11VideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Log whatever usage we measured, if any.
  LogPictureBufferUsage();

  // Explicitly destroy the decoder, since it can reference picture buffers.
  accelerated_video_decoder_.reset();
}

VideoDecoderType D3D11VideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kD3D11;
}

bool D3D11VideoDecoder::InitializeAcceleratedDecoder(
    const VideoDecoderConfig& config) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::InitializeAcceleratedDecoder");

  profile_ = config.profile();
  if (config.codec() == VideoCodec::kVP9) {
    accelerated_video_decoder_ = std::make_unique<VP9Decoder>(
        std::make_unique<D3D11VP9Accelerator>(this, media_log_.get()), profile_,
        config.color_space_info());
  } else if (config.codec() == VideoCodec::kH264) {
    accelerated_video_decoder_ = std::make_unique<H264Decoder>(
        std::make_unique<D3D11H264Accelerator>(this, media_log_.get()),
        profile_, config.color_space_info());
  } else if (config.codec() == VideoCodec::kAV1) {
    accelerated_video_decoder_ = std::make_unique<AV1Decoder>(
        std::make_unique<D3D11AV1Accelerator>(
            this, media_log_.get(),
            gpu_workarounds_.use_current_picture_for_av1_invalid_ref),
        profile_, config.color_space_info());
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  } else if (config.codec() == VideoCodec::kHEVC) {
    DCHECK(base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport));
    accelerated_video_decoder_ = std::make_unique<H265Decoder>(
        std::make_unique<D3D11H265Accelerator>(this, media_log_.get()),
        profile_, config.color_space_info());
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  } else {
    NotifyError(D3D11Status::Codes::kDecoderUnsupportedCodec);
    return false;
  }

  return true;
}

bool D3D11VideoDecoder::ResetD3DVideoDecoder() {
  // By default we assume outputs are 8-bit for SDR color spaces and 10 bit for
  // HDR color spaces (or VP9.2, or HEVC Main10, or HEVC Rext) with HBD capable
  // codecs (the decoder doesn't support H264PROFILE_HIGH10PROFILE). We'll get
  // a config change once we know the real bit depth if this turns out to be
  // wrong.
  uint8_t bit_depth = 0;
  if (accelerated_video_decoder_) {
    bit_depth = accelerated_video_decoder_->GetBitDepth();
  }
  if (!bit_depth) {
    bit_depth =
        (config_.profile() == VP9PROFILE_PROFILE2 ||
                 config_.profile() == HEVCPROFILE_REXT ||
                 config_.profile() == HEVCPROFILE_MAIN10 ||
                 (config_.color_space_info().GuessGfxColorSpace().IsHDR() &&
                  config_.codec() != VideoCodec::kH264 &&
                  config_.profile() != HEVCPROFILE_MAIN &&
                  config_.profile() != HEVCPROFILE_MAIN_STILL_PICTURE)
             ? 10
             : 8);
  }

  auto decoder_configurator = D3D11DecoderConfigurator::Create(
      gpu_preferences_, gpu_workarounds_, config_, bit_depth, chroma_sampling_,
      media_log_.get(), use_shared_handle_);
  if (!decoder_configurator) {
    NotifyError(D3D11StatusCode::kDecoderUnsupportedProfile);
    return false;
  }

  if (!decoder_configurator->SupportsDevice(video_device_)) {
    NotifyError(D3D11StatusCode::kDecoderUnsupportedCodec);
    return false;
  }

  FormatSupportChecker format_checker(device_);
  if (!format_checker.Initialize()) {
    // Don't fail; it'll just return no support a lot.
    MEDIA_LOG(WARNING, media_log_)
        << "Could not create format checker, continuing";
  }

  auto texture_selector = TextureSelector::Create(
      gpu_preferences_, gpu_workarounds_, decoder_configurator->TextureFormat(),
      &format_checker, video_device_, device_context_, media_log_.get(),
      config_.color_space_info().ToGfxColorSpace(), use_shared_handle_);
  if (!texture_selector) {
    NotifyError(D3D11StatusCode::kCreateTextureSelectorFailed);
    return false;
  }

  auto video_decoder_wrapper =
      CreateD3DVideoDecoderWrapper(decoder_configurator.get(), bit_depth);
  if (!video_decoder_wrapper) {
    return false;
  }

  // Replace the re-created members after all error-checking passes.
  bit_depth_ = bit_depth;
  decoder_configurator_ = std::move(decoder_configurator);
  texture_selector_ = std::move(texture_selector);
  d3d_video_decoder_wrapper_ = std::move(video_decoder_wrapper);

  return true;
}

std::unique_ptr<D3DVideoDecoderWrapper>
D3D11VideoDecoder::CreateD3DVideoDecoderWrapper(
    D3D11DecoderConfigurator* decoder_configurator,
    uint8_t bit_depth) {
  CHECK(decoder_configurator);
  std::unique_ptr<D3DVideoDecoderWrapper> video_decoder_wrapper;
  if (base::FeatureList::IsEnabled(kD3D12VideoDecoder)) {
    MEDIA_LOG(INFO, media_log_) << "D3D11VideoDecoder is using D3D12 backend";
    ComUnknown d3d_device = get_d3d_device_cb_.Run(D3DVersion::kD3D12);
    if (!d3d_device) {
      NotifyError({D3D11StatusCode::kUnsupportedFeatureLevel,
                   "Cannot create D3D12Device"});
      return nullptr;
    }

    ComD3D12Device device;
    CHECK_EQ(d3d_device.As(&device), S_OK);

    ComD3D12VideoDevice video_device;
    HRESULT hr = device.As(&video_device);
    if (FAILED(hr)) {
      NotifyError({D3D11StatusCode::kFailedToGetVideoDevice,
                   "Cannot create D3D12VideoDevice", hr});
      return nullptr;
    }

    video_decoder_wrapper = D3D12VideoDecoderWrapper::Create(
        media_log_.get(), video_device, config_, bit_depth, chroma_sampling_);
  } else {
    MEDIA_LOG(INFO, media_log_) << "D3D11VideoDecoder is using D3D11 backend";
    ComD3D11VideoContext video_context;
    CHECK_EQ(device_context_.As(&video_context), S_OK);
    video_decoder_wrapper = D3D11VideoDecoderWrapper::Create(
        media_log_.get(), video_device_, std::move(video_context),
        decoder_configurator, usable_feature_level_, config_);
  }

  if (!video_decoder_wrapper) {
    NotifyError({D3D11StatusCode::kDecoderCreationFailed,
                 "D3DVideoDecoderWrapper is not created"});
    return nullptr;
  }

  auto use_single_texture = video_decoder_wrapper->UseSingleTexture();
  if (!use_single_texture.has_value()) {
    NotifyError({D3D11StatusCode::kGetDecoderConfigFailed,
                 "GetSingleTextureRecommended failed"});
    return nullptr;
  }
  use_single_video_decoder_texture_ =
      use_single_texture.value() | use_shared_handle_;
  if (use_single_video_decoder_texture_) {
    MEDIA_LOG(INFO, media_log_) << "D3D11VideoDecoder is using single textures";
  } else {
    MEDIA_LOG(INFO, media_log_) << "D3D11VideoDecoder is using array texture";
  }

  return video_decoder_wrapper;
}

void D3D11VideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   bool low_delay,
                                   CdmContext* /* cdm_context */,
                                   InitCB init_cb,
                                   const OutputCB& output_cb,
                                   const WaitingCB& /* waiting_cb */) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::Initialize");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_cb);

  state_ = State::kInitializing;

  config_ = config;
  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;

  // Verify that |config| matches one of the supported configurations.  This
  // helps us skip configs that are supported by the VDA but not us, since
  // GpuMojoMediaClient merges them.  This is not hacky, even in the tiniest
  // little bit, nope.  Definitely not.  Convinced?
  bool is_supported = false;
  for (const auto& supported_config : supported_configs_) {
    if (supported_config.Matches(config)) {
      is_supported = true;
      break;
    }
  }

  // If we don't have support support for a given codec, try to initialize
  // anyways -- otherwise we're certain to fail playback.
  if (gpu_workarounds_.disable_d3d11_video_decoder ||
      (!is_supported && IsBuiltInVideoCodec(config.codec()))) {
    return PostDecoderStatus(
        DecoderStatus(DecoderStatus::Codes::kUnsupportedConfig)
            .WithData("config", config));
  }

  if (config.is_encrypted()) {
    return PostDecoderStatus(DecoderStatus::Codes::kUnsupportedEncryptionMode);
  }

  // Initialize the video decoder.

  // Note that we assume that this is the ANGLE device, since we don't implement
  // texture sharing properly.  That also implies that this is the GPU main
  // thread, since we use non-threadsafe properties of the device (e.g., we get
  // the immediate context).
  //
  // Also note that we don't technically have a guarantee that the ANGLE device
  // will use the most recent version of D3D11; it might be configured to use
  // D3D9.  In practice, though, it seems to use 11.1 if it's available, unless
  // it's been specifically configured via switch to avoid d3d11.
  //
  // TODO(liberato): On re-init, we can probably re-use the device.
  // TODO(liberato): This isn't allowed off the main thread, since the callback
  // does who-knows-what.  Either we should be given the angle device, or we
  // should thread-hop to get it.
  ComUnknown d3d_device = get_d3d_device_cb_.Run(D3DVersion::kD3D11);
  if (!d3d_device) {
    // This happens if, for example, if chrome is configured to use
    // D3D9 for ANGLE.
    return NotifyError(D3D11Status::Codes::kFailedToGetAngleDevice);
  }
  CHECK_EQ(d3d_device.As(&device_), S_OK);

  if (!GetD3D11FeatureLevel(device_, gpu_workarounds_,
                            &usable_feature_level_)) {
    return NotifyError(D3D11Status::Codes::kUnsupportedFeatureLevel);
  }

  device_->GetImmediateContext(&device_context_);

  // TODO(liberato): Handle cleanup better.  Also consider being less chatty in
  // the logs, since this will fall back.

  auto hr = device_.As(&video_device_);
  if (FAILED(hr))
    return NotifyError({D3D11Status::Codes::kFailedToGetVideoDevice, hr});

  if (!InitializeAcceleratedDecoder(config_)) {
    return;
  }

  if (!ResetD3DVideoDecoder()) {
    return;
  }

  LogDecoderAdapterLUID();

  // At this point, playback is supported so add a line in the media log to help
  // us figure that out.
  MEDIA_LOG(INFO, media_log_) << "Video is supported by D3D11VideoDecoder";

  // Initialize `mailbox_release_helper_` so we have a ReleaseMailboxCB which
  // knows how to wait for SyncToken resolution. No need to reinitialize if
  // we've done it once.
  if (release_mailbox_cb_) {
    OnGpuInitComplete(true, release_mailbox_cb_);
    return;
  }

  auto mailbox_helper_init_cb = base::BindOnce(
      &D3D11VideoDecoder::OnGpuInitComplete, weak_factory_.GetWeakPtr());
  if (gpu_task_runner_->BelongsToCurrentThread()) {
    mailbox_release_helper_->Initialize(std::move(mailbox_helper_init_cb));
  } else {
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&D3D11VideoFrameMailboxReleaseHelper::Initialize,
                       mailbox_release_helper_,
                       base::BindPostTaskToCurrentDefault(
                           std::move(mailbox_helper_init_cb))));
  }
}

void D3D11VideoDecoder::ReceivePictureBufferFromClient(
    scoped_refptr<D3D11PictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::ReceivePictureBufferFromClient");

  // We may decode into this buffer again.
  // Note that |buffer| might no longer be in |picture_buffers_| if we've
  // replaced them.  That's okay.
  buffer->remove_client_use();

  // Also re-start decoding in case it was waiting for more pictures.
  DoDecode();
}

void D3D11VideoDecoder::PictureBufferGPUResourceInitDone(
    scoped_refptr<D3D11PictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::PictureBufferGPUResourceInitDone");

  buffer->remove_client_use();

  // Picture buffer gpu resource may not be ready when D3D11VideoDecoder
  // initialization finished. In that case, use PictureBuffer::in_client_use()
  // to pause decoder through media::AcceleratedVideoDecoder::kRanOutOfSurfaces
  // state. Then restart decoding after picture buffer gpu resource
  // initialization finished.
  if (state_ == State::kRunning) {
    DoDecode();
  }
}

void D3D11VideoDecoder::OnGpuInitComplete(
    bool success,
    D3D11VideoFrameMailboxReleaseHelper::ReleaseMailboxCB release_mailbox_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::OnGpuInitComplete");

  if (!init_cb_) {
    // We already failed, so just do nothing.
    DCHECK_EQ(state_, State::kError);
    return;
  }

  DCHECK_EQ(state_, State::kInitializing);

  if (!success) {
    return NotifyError(D3D11Status::Codes::kFailedToInitializeGPUProcess);
  }

  release_mailbox_cb_ = std::move(release_mailbox_cb);

  state_ = State::kRunning;
  std::move(init_cb_).Run(DecoderStatus::Codes::kOk);
}

void D3D11VideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::Decode");

  // If we aren't given a decode cb, then record that.
  // crbug.com/1012464 .
  if (!decode_cb)
    base::debug::DumpWithoutCrashing();

  if (state_ == State::kError) {
    // TODO(liberato): consider posting, though it likely doesn't matter.
    std::move(decode_cb).Run(DecoderStatus::Codes::kInterrupted);
    return;
  }

  const bool is_spatial_layer_buffer =
      !buffer->end_of_stream() && buffer->has_side_data() &&
      !buffer->side_data()->spatial_layers.empty();

  input_buffer_queue_.push_back(
      std::make_pair(std::move(buffer), std::move(decode_cb)));

  if (config_.codec() == VideoCodec::kVP9 && is_spatial_layer_buffer &&
      gpu_workarounds_.disable_d3d11_vp9_ksvc_decoding) {
    PostDecoderStatus(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  // Post, since we're not supposed to call back before this returns.  It
  // probably doesn't matter since we're in the gpu process anyway.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoder::DoDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::DoDecode");

  if (state_ != State::kRunning) {
    DVLOG(2) << __func__ << ": Do nothing in " << static_cast<int>(state_)
             << " state.";
    return;
  }

  // Periodically measure picture buffer usage.  We could do this on every free,
  // but it's not that important that we should run it so often.
  if (picture_buffers_.size() > 0) {
    if (!decode_count_until_picture_buffer_measurement_) {
      MeasurePictureBufferUsage();
      decode_count_until_picture_buffer_measurement_ = picture_buffers_.size();
    } else {
      decode_count_until_picture_buffer_measurement_--;
    }
  }

  if (!current_buffer_) {
    if (input_buffer_queue_.empty()) {
      return;
    }
    current_buffer_ = std::move(input_buffer_queue_.front().first);
    current_decode_cb_ = std::move(input_buffer_queue_.front().second);
    // If we pop a null decode cb off the stack, record it so we can see if this
    // is from a top-level call, or through Decode.
    // crbug.com/1012464 .
    if (!current_decode_cb_)
      base::debug::DumpWithoutCrashing();
    input_buffer_queue_.pop_front();
    if (current_buffer_->end_of_stream()) {
      // Flush, then signal the decode cb once all pictures have been output.
      current_buffer_ = nullptr;
      if (!accelerated_video_decoder_->Flush()) {
        // This will also signal error |current_decode_cb_|.
        NotifyError(D3D11Status::Codes::kAcceleratorFlushFailed);
        return;
      }
      // Pictures out output synchronously during Flush.  Signal the decode
      // cb now.
      std::move(current_decode_cb_).Run(DecoderStatus::Codes::kOk);
      return;
    }
    // This must be after checking for EOS because there is no timestamp for an
    // EOS buffer.
    current_timestamp_ = current_buffer_->timestamp();

    accelerated_video_decoder_->SetStream(-1, *current_buffer_);
  }

  while (true) {
    // If we transition to the error state, then stop here.
    if (state_ == State::kError)
      return;

    // If somebody cleared the buffer, then stop and post.
    // TODO(liberato): It's unclear to me how this might happen.  If it does
    // fix the crash, then more investigation is required.  Please see
    // crbug.com/1012464 for more information.
    if (!current_buffer_)
      break;

    // Record if we get here with a buffer, but without a decode cb.  This
    // shouldn't happen, but does.  This will prevent the crash, and record how
    // we got here.
    // crbug.com/1012464 .
    if (!current_decode_cb_) {
      base::debug::DumpWithoutCrashing();
      current_buffer_ = nullptr;
      break;
    }

    media::AcceleratedVideoDecoder::DecodeResult result =
        accelerated_video_decoder_->Decode();
    if (state_ == State::kError) {
      // Transitioned to an error at some point.  The h264 accelerator can do
      // this if picture output fails, at least.  Until that's fixed, check
      // here and exit if so.
      return;
    }
    // TODO(liberato): switch + class enum.
    if (result == media::AcceleratedVideoDecoder::kRanOutOfStreamData) {
      current_buffer_ = nullptr;
      std::move(current_decode_cb_).Run(DecoderStatus::Codes::kOk);
      break;
    } else if (result == media::AcceleratedVideoDecoder::kRanOutOfSurfaces) {
      // At this point, we know the picture size.
      // If we haven't allocated picture buffers yet, then allocate some now.
      // Otherwise, stop here.  We'll restart when a picture comes back.
      if (picture_buffers_.size())
        return;

      CreatePictureBuffers();
    } else if (result == media::AcceleratedVideoDecoder::kConfigChange) {
      // Before the first frame, we get a config change that we should ignore.
      // We only want to take action if this is a mid-stream config change.  We
      // could wait until now to allocate the first D3D11VideoDecoder, but we
      // don't, so that init can fail rather than decoding if there's a problem
      // creating it.  We could also unconditionally re-allocate the decoder,
      // but we keep it if it's ready to go.
      const auto new_bit_depth = accelerated_video_decoder_->GetBitDepth();
      const auto new_profile = accelerated_video_decoder_->GetProfile();
      const auto new_coded_size = accelerated_video_decoder_->GetPicSize();
      const auto new_chroma_sampling =
          accelerated_video_decoder_->GetChromaSampling();
      const auto new_color_space =
          accelerated_video_decoder_->GetVideoColorSpace();
      if (new_profile == config_.profile() &&
          new_coded_size == config_.coded_size() &&
          new_bit_depth == bit_depth_ && !picture_buffers_.size() &&
          new_chroma_sampling == chroma_sampling_ &&
          new_color_space == color_space_) {
        continue;
      }

      // Update the config.
      MEDIA_LOG(INFO, media_log_)
          << "D3D11VideoDecoder config change: profile: "
          << GetProfileName(new_profile) << ", chroma_sampling_format: "
          << VideoChromaSamplingToString(new_chroma_sampling)
          << ", coded_size: " << new_coded_size.ToString()
          << ", bit_depth: " << base::strict_cast<int>(new_bit_depth)
          << ", color_space: " << new_color_space.ToString();
      profile_ = new_profile;
      config_.set_profile(profile_);
      config_.set_coded_size(new_coded_size);
      chroma_sampling_ = new_chroma_sampling;
      color_space_ = new_color_space;

      // Replace the decoder, and clear any picture buffers we have.  It's okay
      // if we don't have any picture buffer yet; this might be before the
      // accelerated decoder asked for any.
      if (!ResetD3DVideoDecoder()) {
        return;
      }
      picture_buffers_.clear();
    } else if (result == media::AcceleratedVideoDecoder::kTryAgain) {
      LOG(ERROR) << "Try again is not supported";
      NotifyError(D3D11Status::Codes::kTryAgainNotSupported);
      return;
    } else {
      return NotifyError(D3D11Status(D3D11Status::Codes::kDecoderFailedDecode)
                             .WithData("VDA Error", result));
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitializing);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::Reset");

  current_buffer_ = nullptr;
  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecoderStatus::Codes::kAborted);

  for (auto& queue_pair : input_buffer_queue_)
    std::move(queue_pair.second).Run(DecoderStatus::Codes::kAborted);
  input_buffer_queue_.clear();

  // TODO(liberato): how do we signal an error?
  accelerated_video_decoder_->Reset();

  std::move(closure).Run();
}

bool D3D11VideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return true;
}

bool D3D11VideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If picture buffers haven't been created yet, the client can read.
  if (picture_buffers_.empty()) {
    return true;
  }

  // If we haven't given all our picture buffers to the client, it can read.
  for (const auto& buffer : picture_buffers_) {
    if (!buffer->in_client_use()) {
      return true;
    }
  }

  return false;
}

int D3D11VideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return 4;
}

void D3D11VideoDecoder::CreatePictureBuffers() {
  // TODO(liberato): When we run off the gpu main thread, this call will need
  // to signal success / failure asynchronously.  We'll need to transition into
  // a "waiting for pictures" state, since D3D11PictureBuffer will post the gpu
  // thread work.
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::CreatePictureBuffers");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(decoder_configurator_);
  DCHECK(texture_selector_);
  gfx::Size size = accelerated_video_decoder_->GetPicSize();
  gfx::ColorSpace color_space =
      accelerated_video_decoder_->GetVideoColorSpace().ToGfxColorSpace();
  if (!color_space.IsValid()) {
    color_space = config_.color_space_info().ToGfxColorSpace();
  }

  // Since we are about to allocate new picture buffers, record whatever usage
  // we had for the outgoing ones, if any.
  LogPictureBufferUsage();

  // Drop any old pictures.
  for (auto& buffer : picture_buffers_)
    DCHECK(!buffer->in_picture_use());
  picture_buffers_.clear();

  ComD3D11Texture2D in_texture;

  // In addition to what the decoder needs, add one picture buffer
  // for overlay weirdness, just to be safe. We may need to track
  // actual used buffers for all use cases and decide an optimal
  // number of picture buffers.
  size_t pic_buffers_required =
      accelerated_video_decoder_->GetRequiredNumOfPictures() + 1;

  // Create each picture buffer.
  for (size_t i = 0; i < pic_buffers_required; i++) {
    // Create an input texture / texture array if we haven't already.
    if (!in_texture) {
      auto result = decoder_configurator_->CreateOutputTexture(
          device_, size,
          use_single_video_decoder_texture_ ? 1 : pic_buffers_required,
          texture_selector_->DoesDecoderOutputUseSharedHandle());
      if (result.has_value()) {
        in_texture = std::move(result).value();
      } else {
        return NotifyError(std::move(result).error().AddHere());
      }
    }

    DCHECK(!!in_texture);

    auto tex_wrapper =
        texture_selector_->CreateTextureWrapper(device_, color_space, size);
    if (!tex_wrapper) {
      return NotifyError(
          D3D11Status::Codes::kAllocateTextureForCopyingWrapperFailed);
    }

    const size_t array_slice = use_single_video_decoder_texture_ ? 0 : i;
    picture_buffers_.push_back(base::MakeRefCounted<D3D11PictureBuffer>(
        decoder_task_runner_, in_texture, array_slice, std::move(tex_wrapper),
        size, /*level=*/i));

    base::OnceCallback<void(scoped_refptr<media::D3D11PictureBuffer>)>
        picture_buffer_gpu_resource_init_done_cb = base::DoNothing();

    // WebGPU requires interop on the picture buffer to achieve zero copy.
    // This requires a picture buffer to produce a shared image representation
    // during initialization. Add picture buffer in_client_use count to idle
    // the decoder until picture buffer finished gpu resource initialization
    // in gpu thread.
    picture_buffers_[i]->add_client_use();
    picture_buffer_gpu_resource_init_done_cb =
        base::BindPostTaskToCurrentDefault(
            base::BindOnce(&D3D11VideoDecoder::PictureBufferGPUResourceInitDone,
                           weak_factory_.GetWeakPtr()));

    D3D11Status result = picture_buffers_[i]->Init(
        gpu_task_runner_, get_helper_cb_, video_device_,
        decoder_configurator_->DecoderGuid(), media_log_->Clone(),
        std::move(picture_buffer_gpu_resource_init_done_cb));
    if (!result.is_ok()) {
      return NotifyError(std::move(result).AddHere());
    }

    // If we're using one texture per buffer, rather than an array, then clear
    // the ref to it so that we allocate a new one above.
    if (use_single_video_decoder_texture_)
      in_texture = nullptr;
  }
}

D3D11PictureBuffer* D3D11VideoDecoder::GetPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& buffer : picture_buffers_) {
    if (!buffer->in_client_use() && !buffer->in_picture_use()) {
      buffer->timestamp_ = current_timestamp_;
      return buffer.get();
    }
  }

  return nullptr;
}

void D3D11VideoDecoder::UpdateTimestamp(D3D11PictureBuffer* picture_buffer) {
  // A picture is being reused with a different timestamp; since we've already
  // generated a VideoFrame from the previous picture buffer, we can just stamp
  // the new timestamp directly onto the buffer.
  picture_buffer->timestamp_ = current_timestamp_;
}

bool D3D11VideoDecoder::OutputResult(const CodecPicture* picture,
                                     D3D11PictureBuffer* picture_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(texture_selector_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::OutputResult");

  picture_buffer->add_client_use();

  // Note: The pixel format doesn't matter.
  gfx::Rect visible_rect = picture->visible_rect();
  if (visible_rect.IsEmpty())
    visible_rect = config_.visible_rect();

  // TODO(crbug.com/41389060): Use aspect ratio from decoder (SPS) if
  // the config's aspect ratio isn't valid.
  gfx::Size natural_size = config_.aspect_ratio().GetNaturalSize(visible_rect);

  base::TimeDelta timestamp = picture_buffer->timestamp_;

  // Prefer the frame color space over what's in the config.
  auto picture_color_space = picture->get_colorspace().ToGfxColorSpace();
  if (!picture_color_space.IsValid()) {
    picture_color_space = config_.color_space_info().ToGfxColorSpace();
  }

  scoped_refptr<gpu::ClientSharedImage> shared_image;
  D3D11Status result =
      picture_buffer->ProcessTexture(picture_color_space, shared_image);
  if (!result.is_ok()) {
    NotifyError(std::move(result).AddHere());
    return false;
  }

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapSharedImage(
      texture_selector_->PixelFormat(), shared_image,
      shared_image->creation_sync_token(), VideoFrame::ReleaseMailboxCB(),
      picture_buffer->size(), visible_rect, natural_size, timestamp);

  if (!frame) {
    // This can happen if, somehow, we get an unsupported combination of
    // pixel format, etc.
    PostDecoderStatus(DecoderStatus::Codes::kFailedToGetVideoFrame);
    return false;
  }

  // Remember that this will likely thread-hop to the GPU main thread.  Note
  // that |picture_buffer| will delete on sequence, so it's okay even if
  // |wait_complete_cb| doesn't ever run.
  auto wait_complete_cb = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&D3D11VideoDecoder::ReceivePictureBufferFromClient,
                     weak_factory_.GetWeakPtr(),
                     scoped_refptr<D3D11PictureBuffer>(picture_buffer)));
  frame->SetReleaseMailboxCB(
      base::BindOnce(release_mailbox_cb_, std::move(wait_complete_cb)));
  // For NV12, overlay is allowed by default. If the decoder is going to support
  // non-NV12 textures, then this may have to be conditionally set. Also note
  // that ALLOW_OVERLAY is required for encrypted video path.
  //
  // Since all of our picture buffers allow overlay, we just set this to true.
  // However, we may choose to set ALLOW_OVERLAY to false even if
  // the finch flag is enabled.  We may not choose to set ALLOW_OVERLAY if the
  // flag is off, however.
  frame->metadata().allow_overlay = true;
  // The swapchain presenter currently only supports NV12 and P010 overlay,
  // with BGRA only as fallback when DWM continuously fails to overlay submitted
  // video. As a result, we should not allow overlay for non-NV12/P010 formats
  // which may cause chroma downsampling when blitting into the back buffer.
  // See https://crbugs.com/331679628 for more details.
  if (!config_.is_encrypted()) {
    frame->metadata().allow_overlay =
        texture_selector_->OutputDXGIFormat() == DXGI_FORMAT_P010 ||
        texture_selector_->OutputDXGIFormat() == DXGI_FORMAT_NV12;
  }
  frame->metadata().power_efficient = true;

  frame->set_color_space(picture_color_space);
  if (picture_color_space.IsHDR()) {
    // Some streams may have varying metadata, so bitstream metadata should be
    // preferred over metadata provide by the configuration.
    frame->set_hdr_metadata(picture->hdr_metadata() ? picture->hdr_metadata()
                                                    : config_.hdr_metadata());
  }

  frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);

  frame->metadata().is_webgpu_compatible = use_shared_handle_;

  output_cb_.Run(frame);
  return true;
}

D3DVideoDecoderWrapper* D3D11VideoDecoder::GetWrapper() {
  return d3d_video_decoder_wrapper_.get();
}

void D3D11VideoDecoder::NotifyError(D3D11Status reason,
                                    DecoderStatus::Codes opt_decoder_code) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::NotifyError");

  if (!reason.is_ok() && !reason.message().empty()) {
    MEDIA_LOG(ERROR, media_log_) << "D3D11VideoDecoder: " << reason.message();
  }

  PostDecoderStatus(
      DecoderStatus(opt_decoder_code).AddCause(std::move(reason)));
}

void D3D11VideoDecoder::PostDecoderStatus(DecoderStatus status) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::PostDecoderStatus");

  state_ = State::kError;
  current_buffer_ = nullptr;

  if (init_cb_) {
    std::move(init_cb_).Run(status);
  }

  if (current_decode_cb_) {
    std::move(current_decode_cb_).Run(status);
  }

  for (auto& queue_pair : input_buffer_queue_) {
    std::move(queue_pair.second).Run(status);
  }

  // Also clear |input_buffer_queue_| since the callbacks have been consumed.
  input_buffer_queue_.clear();
}

void D3D11VideoDecoder::MeasurePictureBufferUsage() {
  // Count the total number of buffers that are currently unused by either the
  // client or the decoder.  These are buffers that we didn't need to allocate.
  int unused_buffers = 0;
  for (const auto& buffer : picture_buffers_) {
    if (!buffer->in_client_use() && !buffer->in_picture_use())
      unused_buffers++;
  }

  if (!min_unused_buffers_ || unused_buffers < *min_unused_buffers_) {
    min_unused_buffers_ = unused_buffers;
  }
}

void D3D11VideoDecoder::LogPictureBufferUsage() {
  if (!min_unused_buffers_)
    return;

  // Record these separately because (a) we could potentially fix the
  // MultiTexture case pretty easily, and (b) we have no idea how often we're in
  // one mode vs the other.  This will let us know if there is enough usage of
  // MultiTexture and also enough unused textures that it's worth fixing.  Note
  // that this assumes that we would lazily allocate buffers but not free them,
  // and is a lower bound on savings.
  if (use_single_video_decoder_texture_) {
    UMA_HISTOGRAM_COUNTS_100(
        "Media.D3D11VideoDecoder.UnusedPictureBufferCount.SingleTexture",
        *min_unused_buffers_);
  } else {
    UMA_HISTOGRAM_COUNTS_100(
        "Media.D3D11VideoDecoder.UnusedPictureBufferCount.MultiTexture",
        *min_unused_buffers_);
  }

  min_unused_buffers_.reset();
}

void D3D11VideoDecoder::LogDecoderAdapterLUID() {
  if (!device_)
    return;

  ComDXGIDevice dxgi_device;
  HRESULT hr = device_.As(&dxgi_device);
  if (FAILED(hr))
    return;

  ComDXGIAdapter dxgi_adapter;
  hr = dxgi_device->GetAdapter(&dxgi_adapter);
  if (FAILED(hr))
    return;

  DXGI_ADAPTER_DESC adapter_desc{};
  hr = dxgi_adapter->GetDesc(&adapter_desc);
  if (FAILED(hr))
    return;

  MEDIA_LOG(INFO, media_log_) << "Selected D3D11VideoDecoder adapter LUID:{"
                              << adapter_desc.AdapterLuid.HighPart << ", "
                              << adapter_desc.AdapterLuid.LowPart << "}";
}

// static
bool D3D11VideoDecoder::GetD3D11FeatureLevel(
    ComD3D11Device dev,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    D3D_FEATURE_LEVEL* feature_level) {
  if (!dev || !feature_level)
    return false;

  *feature_level = dev->GetFeatureLevel();
  if (*feature_level < D3D_FEATURE_LEVEL_11_0)
    return false;

  if (gpu_workarounds.limit_d3d11_video_decoder_to_11_0)
    *feature_level = D3D_FEATURE_LEVEL_11_0;

  return true;
}

// static
std::vector<SupportedVideoDecoderConfig>
D3D11VideoDecoder::GetSupportedVideoDecoderConfigs(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    GetD3DDeviceCB get_d3d_device_cb) {
  // Allow all of d3d11 to be turned off by workaround.
  if (gpu_workarounds.disable_d3d11_video_decoder)
    return {};

  SupportedResolutionRangeMap supported_resolutions;
  if (base::FeatureList::IsEnabled(kD3D12VideoDecoder)) {
    auto d3d_device = get_d3d_device_cb.Run(D3DVersion::kD3D12);
    if (!d3d_device) {
      return {};
    }
    ComD3D12Device d3d12_device;
    CHECK_EQ(d3d_device.As(&d3d12_device), S_OK);

    supported_resolutions =
        GetSupportedD3D12VideoDecoderResolutions(d3d12_device, gpu_workarounds);
  } else {
    // Remember that this might query the angle device, so this won't work if
    // we're not on the GPU main thread.  Also remember that devices are thread
    // safe (contexts are not), so we could use the angle device from any thread
    // as long as we're not calling into possible not-thread-safe things to get
    // it.  I.e., if this cached it, then it'd be fine.  It's up to our caller
    // to guarantee that, though.
    //
    // Note also that, currently, we are called from the GPU main thread only.
    auto d3d_device = get_d3d_device_cb.Run(D3DVersion::kD3D11);
    if (!d3d_device) {
      return {};
    }
    ComD3D11Device d3d11_device;
    CHECK_EQ(d3d_device.As(&d3d11_device), S_OK);

    D3D_FEATURE_LEVEL usable_feature_level;
    if (!GetD3D11FeatureLevel(d3d11_device, gpu_workarounds,
                              &usable_feature_level)) {
      return {};
    }

    supported_resolutions =
        GetSupportedD3D11VideoDecoderResolutions(d3d11_device, gpu_workarounds);
  }

  std::vector<SupportedVideoDecoderConfig> configs;
  for (const auto& kv : supported_resolutions) {
    const auto profile = kv.first;
    // TODO(liberato): Add VP8 support to D3D11VideoDecoder.
    if (profile == VP8PROFILE_ANY)
      continue;

    const auto& resolution_range = kv.second;
    configs.emplace_back(profile, profile, resolution_range.min_resolution,
                         resolution_range.max_landscape_resolution,
                         /*allow_encrypted=*/false,
                         /*require_encrypted=*/false);
    if (!resolution_range.max_portrait_resolution.IsEmpty() &&
        resolution_range.max_portrait_resolution !=
            resolution_range.max_landscape_resolution) {
      configs.emplace_back(profile, profile, resolution_range.min_resolution,
                           resolution_range.max_portrait_resolution,
                           /*allow_encrypted=*/false,
                           /*require_encrypted=*/false);
    }
  }

  return configs;
}

}  // namespace media
