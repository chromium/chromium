// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d_video_decoder.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
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
#include "media/gpu/windows/d3d11_texture_selector.h"
#include "media/gpu/windows/d3d11_video_decoder_backend.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
#include "media/gpu/windows/d3d_av1_accelerator.h"
#include "media/gpu/windows/d3d_decoder_configurator.h"
#include "media/gpu/windows/d3d_picture_buffer.h"
#include "media/gpu/windows/d3d_status.h"
#include "media/gpu/windows/d3d_video_frame_mailbox_release_helper.h"
#include "media/media_buildflags.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/hdr_metadata.h"

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/gpu/windows/d3d_h265_accelerator.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

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
  if (!stub) {
    return nullptr;
  }

  DCHECK(holder);
  if (!holder->helper) {
    holder->helper = CommandBufferHelper::Create(stub);
  }

  return holder->helper;
}

std::unique_ptr<D3DVideoDecoderBackend> CreateVideoDecoderBackend() {
  return std::make_unique<D3D11VideoDecoderBackend>();
}

}  // namespace

std::unique_ptr<VideoDecoder> D3DVideoDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb,
    GetD3DDeviceCB get_d3d_device_cb,
    SupportedConfigs supported_configs) {
  // Note that the output callback will hop to our thread, post the video
  // frame, and along with a callback that will hop back to the impl thread
  // when it's released.
  // Note that we WrapUnique<VideoDecoder> rather than D3DVideoDecoder to make
  // this castable; the deleters have to match.
  auto get_helper_cb = base::BindRepeating(
      CreateCommandBufferHelper, std::move(get_stub_cb),
      base::MakeRefCounted<CommandBufferHelperHolder>(gpu_task_runner));
  return base::WrapUnique<VideoDecoder>(new D3DVideoDecoder(
      gpu_task_runner, std::move(media_log), gpu_preferences, gpu_workarounds,
      get_helper_cb, std::move(get_d3d_device_cb), CreateVideoDecoderBackend(),
      std::move(supported_configs)));
}

D3DVideoDecoder::D3DVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb,
    GetD3DDeviceCB get_d3d_device_cb,
    std::unique_ptr<D3DVideoDecoderBackend> backend,
    SupportedConfigs supported_configs)
    : media_log_(std::move(media_log)),
      mailbox_release_helper_(
          base::MakeRefCounted<D3DVideoFrameMailboxReleaseHelper>(
              media_log_->Clone(),
              get_helper_cb)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      decoder_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      get_d3d_device_cb_(std::move(get_d3d_device_cb)),
      backend_(std::move(backend)),
      get_helper_cb_(std::move(get_helper_cb)),
      supported_configs_(std::move(supported_configs)),
      use_shared_handle_(
          base::FeatureList::IsEnabled(kD3D12VideoDecoder) ||
          base::FeatureList::IsEnabled(kD3D11VideoDecoderUseSharedHandle)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_log_);
  CHECK(backend_);
}

D3DVideoDecoder::~D3DVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Log whatever usage we measured, if any.
  LogPictureBufferUsage();

  // Declaration order in the header means that `backend` will be destroyed
  // before `accelerated_video_decoder_` so that we can complete any D3D
  // specific cleanup before the accelerated decoder is destroyed.
  // `accelerated_video_decoder_` will be destroyed before `picture_buffers_`
  // since it can reference picture buffers.
}

VideoDecoderType D3DVideoDecoder::GetDecoderType() const {
  return backend_->GetDecoderType();
}

bool D3DVideoDecoder::InitializeAcceleratedDecoder(
    const VideoDecoderConfig& config) {
  TRACE_EVENT0("gpu", "D3DVideoDecoder::InitializeAcceleratedDecoder");

  profile_ = config.profile();
  if (config.codec() == VideoCodec::kVP9) {
    accelerated_video_decoder_ = std::make_unique<VP9Decoder>(
        std::make_unique<D3DVP9Accelerator>(this, media_log_.get()), profile_,
        config.color_space_info());
  } else if (config.codec() == VideoCodec::kH264) {
    accelerated_video_decoder_ = std::make_unique<H264Decoder>(
        std::make_unique<D3DH264Accelerator>(this, media_log_.get()), profile_,
        config.color_space_info());
  } else if (config.codec() == VideoCodec::kAV1) {
    accelerated_video_decoder_ = std::make_unique<AV1Decoder>(
        std::make_unique<D3DAV1Accelerator>(
            this, media_log_.get(),
            gpu_workarounds_.use_first_valid_ref_for_av1_invalid_ref),
        profile_, config.color_space_info());
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  } else if (config.codec() == VideoCodec::kHEVC) {
    DCHECK(base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport));
    bool use_dxva_device_for_hevc_rext =
        backend_->ShouldUseDXVADeviceForHEVCRangeExtension(config);
    accelerated_video_decoder_ = std::make_unique<H265Decoder>(
        std::make_unique<D3DH265Accelerator>(this, media_log_.get(),
                                             use_dxva_device_for_hevc_rext),
        profile_, config.color_space_info());
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  } else {
    NotifyError(D3DStatus::Codes::kDecoderUnsupportedCodec);
    return false;
  }

  return true;
}

bool D3DVideoDecoder::RecreateDecoderWrapper() {
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

  auto decoder_configurator_result = backend_->CreateDecoderConfigurator(
      bit_depth, config_, chroma_sampling_, gpu_preferences_, gpu_workarounds_,
      use_shared_handle_, media_log_.get());
  if (!decoder_configurator_result.has_value()) {
    NotifyError(std::move(decoder_configurator_result).error().AddHere());
    return false;
  }
  auto decoder_configurator = std::move(decoder_configurator_result).value();

  auto texture_selector_result = backend_->CreateTextureSelector(
      decoder_configurator.get(), config_, gpu_preferences_, gpu_workarounds_,
      use_shared_handle_, media_log_.get());
  if (!texture_selector_result.has_value()) {
    NotifyError(std::move(texture_selector_result).error().AddHere());
    return false;
  }
  auto texture_selector = std::move(texture_selector_result).value();

  auto video_decoder_wrapper_result = backend_->CreateVideoDecoderWrapper(
      get_d3d_device_cb_, decoder_configurator.get(), config_, bit_depth,
      chroma_sampling_, GetMaxDecodeRequests(), media_log_.get());
  if (!video_decoder_wrapper_result.has_value()) {
    NotifyError(std::move(video_decoder_wrapper_result).error().AddHere());
    return false;
  }
  auto video_decoder_wrapper = std::move(video_decoder_wrapper_result).value();
  if (!video_decoder_wrapper) {
    NotifyError({D3DStatusCode::kDecoderCreationFailed,
                 "D3DVideoDecoderWrapper is not created"});
    return false;
  }

  auto use_single_texture = video_decoder_wrapper->UseSingleTexture();
  if (!use_single_texture.has_value()) {
    NotifyError({D3DStatusCode::kGetDecoderConfigFailed,
                 "GetSingleTextureRecommended failed"});
    return false;
  }
  use_single_video_decoder_texture_ =
      base::FeatureList::IsEnabled(kD3D11VideoDecoderForceSingleTexture) ||
      use_single_texture.value() || use_shared_handle_ ||
      gpu_workarounds_.disable_decode_into_array_texture;
  if (use_single_video_decoder_texture_) {
    MEDIA_LOG(INFO, media_log_) << "D3DVideoDecoder is using single textures";
  } else {
    MEDIA_LOG(INFO, media_log_) << "D3DVideoDecoder is using array texture";
  }

  // Replace the re-created members after all error-checking passes.
  bit_depth_ = bit_depth;
  decoder_configurator_ = std::move(decoder_configurator);
  texture_selector_ = std::move(texture_selector);
  d3d_video_decoder_wrapper_ = std::move(video_decoder_wrapper);

  return true;
}

void D3DVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool low_delay,
                                 CdmContext* /* cdm_context */,
                                 InitCB init_cb,
                                 const OutputCB& output_cb,
                                 const WaitingCB& /* waiting_cb */) {
  TRACE_EVENT0("gpu", "D3DVideoDecoder::Initialize");

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
      (!is_supported && IsDecoderBuiltInVideoCodec(config.codec()))) {
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
  // TODO(liberato): This isn't allowed off the main thread, since the callback
  // does who-knows-what.  Either we should be given the angle device, or we
  // should thread-hop to get it.
  D3DStatus result = backend_->AcquireDeviceResources(get_d3d_device_cb_);
  if (!result.is_ok()) {
    return NotifyError(std::move(result).AddHere());
  }

  if (!InitializeAcceleratedDecoder(config_)) {
    return;
  }

  if (!RecreateDecoderWrapper()) {
    return;
  }

  backend_->LogDecoderAdapterInfo(media_log_.get());

  // At this point, playback is supported so add a line in the media log to help
  // us figure that out.
  MEDIA_LOG(INFO, media_log_) << "Video is supported by D3DVideoDecoder";

  // Initialize `mailbox_release_helper_` so we have a ReleaseMailboxCB which
  // knows how to wait for SyncToken resolution. No need to reinitialize if
  // we've done it once.
  if (release_mailbox_cb_) {
    OnGpuInitComplete(true, release_mailbox_cb_);
    return;
  }

  auto mailbox_helper_init_cb = base::BindOnce(
      &D3DVideoDecoder::OnGpuInitComplete, weak_factory_.GetWeakPtr());
  if (gpu_task_runner_->BelongsToCurrentThread()) {
    mailbox_release_helper_->Initialize(std::move(mailbox_helper_init_cb));
  } else {
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&D3DVideoFrameMailboxReleaseHelper::Initialize,
                       mailbox_release_helper_,
                       base::BindPostTaskToCurrentDefault(
                           std::move(mailbox_helper_init_cb))));
  }
}

void D3DVideoDecoder::ReceivePictureBufferFromClient(
    scoped_refptr<D3DPictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3DVideoDecoder::ReceivePictureBufferFromClient");

  // We may decode into this buffer again.
  // Note that |buffer| might no longer be in |picture_buffers_| if we've
  // replaced them.  That's okay.
  buffer->remove_client_use();

  // Also re-start decoding in case it was waiting for more pictures.
  DoDecode();
}

void D3DVideoDecoder::PictureBufferGPUResourceInitDone(
    scoped_refptr<D3DPictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3DVideoDecoder::PictureBufferGPUResourceInitDone");

  buffer->remove_client_use();

  // Picture buffer gpu resource may not be ready when D3DVideoDecoder
  // initialization finished. In that case, use PictureBuffer::in_client_use()
  // to pause decoder through media::AcceleratedVideoDecoder::kRanOutOfSurfaces
  // state. Then restart decoding after picture buffer gpu resource
  // initialization finished.
  if (state_ == State::kRunning) {
    DoDecode();
  }
}

void D3DVideoDecoder::OnGpuInitComplete(
    bool success,
    D3DVideoFrameMailboxReleaseHelper::ReleaseMailboxCB release_mailbox_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3DVideoDecoder::OnGpuInitComplete");

  if (!init_cb_) {
    // We already failed, so just do nothing.
    DCHECK_EQ(state_, State::kError);
    return;
  }

  DCHECK_EQ(state_, State::kInitializing);

  if (!success) {
    return NotifyError(D3DStatus::Codes::kFailedToInitializeGPUProcess);
  }

  release_mailbox_cb_ = std::move(release_mailbox_cb);

  state_ = State::kRunning;
  std::move(init_cb_).Run(DecoderStatus::Codes::kOk);
}

void D3DVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3DVideoDecoder::Decode");

  // If we aren't given a decode cb, then record that.
  // crbug.com/1012464 .
  if (!decode_cb) {
    base::debug::DumpWithoutCrashing();
  }

  if (state_ == State::kError) {
    // TODO(liberato): consider posting, though it likely doesn't matter.
    std::move(decode_cb).Run(DecoderStatus::Codes::kInterrupted);
    return;
  }

  const bool is_spatial_layer_buffer =
      !buffer->end_of_stream() && buffer->side_data() &&
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
      base::BindOnce(&D3DVideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3DVideoDecoder::DoDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3DVideoDecoder::DoDecode");

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
    if (!current_decode_cb_) {
      base::debug::DumpWithoutCrashing();
    }
    input_buffer_queue_.pop_front();
    if (current_buffer_->end_of_stream()) {
      // Flush, then signal the decode cb once all pictures have been output.
      current_buffer_ = nullptr;
      if (!accelerated_video_decoder_->Flush()) {
        // This will also signal error |current_decode_cb_|.
        NotifyError(D3DStatus::Codes::kAcceleratorFlushFailed);
        return;
      }
      // Pictures out output synchronously during Flush.  Signal the decode
      // cb now.
      std::move(current_decode_cb_).Run(DecoderStatus::Codes::kOk);
      return;
    } else if (current_buffer_->empty()) {
      // Treat an empty buffer as no-op.
      current_buffer_ = nullptr;
      std::move(current_decode_cb_).Run(DecoderStatus::Codes::kOk);
      return;
    }
    // This must be after checking for EOS because there is no timestamp for an
    // EOS buffer.
    current_timestamp_ = current_buffer_->timestamp();

    accelerated_video_decoder_->SetStream(-1, current_buffer_);
  }

  while (true) {
    // If we transition to the error state, then stop here.
    if (state_ == State::kError) {
      return;
    }

    // If somebody cleared the buffer, then stop and post.
    // TODO(liberato): It's unclear to me how this might happen.  If it does
    // fix the crash, then more investigation is required.  Please see
    // crbug.com/1012464 for more information.
    if (!current_buffer_) {
      break;
    }

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
      if (picture_buffers_.size()) {
        return;
      }

      CreatePictureBuffers();
    } else if (result == media::AcceleratedVideoDecoder::kConfigChange) {
      // Before the first frame, we get a config change that we should ignore.
      // We only want to take action if this is a mid-stream config change.  We
      // could wait until now to allocate the first D3DVideoDecoder, but we
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
      const auto new_visible_rect =
          accelerated_video_decoder_->GetVisibleRect();
      DCHECK(gfx::Rect(new_coded_size).Contains(new_visible_rect));
      DCHECK(!new_visible_rect.IsEmpty());
      if (new_profile == config_.profile() &&
          new_coded_size == config_.coded_size() &&
          new_visible_rect == config_.visible_rect() &&
          new_bit_depth == bit_depth_ && !picture_buffers_.size() &&
          new_chroma_sampling == chroma_sampling_ &&
          new_color_space == color_space_) {
        continue;
      }

      // Update the config.
      MEDIA_LOG(INFO, media_log_)
          << "D3DVideoDecoder config change: profile: "
          << GetProfileName(new_profile) << ", chroma_sampling_format: "
          << VideoChromaSamplingToString(new_chroma_sampling)
          << ", coded_size: " << new_coded_size.ToString()
          << ", visible_rect: " << new_visible_rect.ToString()
          << ", bit_depth: " << base::strict_cast<int>(new_bit_depth)
          << ", color_space: " << new_color_space.ToString();
      profile_ = new_profile;
      config_.set_profile(profile_);
      config_.set_coded_size(new_coded_size);
      config_.set_visible_rect(new_visible_rect);
      chroma_sampling_ = new_chroma_sampling;
      color_space_ = new_color_space;

      // Replace the decoder, and clear any picture buffers we have.  It's okay
      // if we don't have any picture buffer yet; this might be before the
      // accelerated decoder asked for any.
      if (!RecreateDecoderWrapper()) {
        return;
      }
      picture_buffers_.clear();
    } else if (result == media::AcceleratedVideoDecoder::kTryAgain) {
      LOG(ERROR) << "Try again is not supported";
      NotifyError(D3DStatus::Codes::kTryAgainNotSupported);
      return;
    } else {
      return NotifyError(D3DStatus(D3DStatus::Codes::kDecoderFailedDecode)
                             .WithData("VDA Error", result));
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3DVideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3DVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitializing);
  TRACE_EVENT0("gpu", "D3DVideoDecoder::Reset");

  // TODO(liberato): how do we signal an error?
  accelerated_video_decoder_->Reset();

  current_buffer_ = nullptr;
  if (current_decode_cb_) {
    std::move(current_decode_cb_).Run(DecoderStatus::Codes::kAborted);
  }

  for (auto& queue_pair : input_buffer_queue_) {
    std::move(queue_pair.second).Run(DecoderStatus::Codes::kAborted);
  }
  input_buffer_queue_.clear();

  std::move(closure).Run();
}

bool D3DVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return true;
}

bool D3DVideoDecoder::CanReadWithoutStalling() const {
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

int D3DVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return 4;
}

void D3DVideoDecoder::CreatePictureBuffers() {
  // TODO(liberato): When we run off the gpu main thread, this call will need
  // to signal success / failure asynchronously.  We'll need to transition into
  // a "waiting for pictures" state, since D3DPictureBuffer will post the gpu
  // thread work.
  TRACE_EVENT0("gpu", "D3DVideoDecoder::CreatePictureBuffers");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(decoder_configurator_);
  DCHECK(texture_selector_);
  gfx::Size size = accelerated_video_decoder_->GetPicSize();
  gfx::ColorSpace color_space =
      accelerated_video_decoder_->GetVideoColorSpace().ToGfxColorSpace();
  if (!color_space.IsValid()) {
    color_space = config_.color_space_info().ToGfxColorSpace();
  }
  if (!color_space.IsValid()) {
    auto output_si_format = texture_selector_->OutputSharedImageFormat();
    // Use BT709 as the default color space for multi-planar formats and SRGB
    // for single-planar.
    color_space = output_si_format.is_multi_plane()
                      ? gfx::ColorSpace::CreateREC709()
                      : gfx::ColorSpace::CreateSRGB();
  }

  // Since we are about to allocate new picture buffers, record whatever usage
  // we had for the outgoing ones, if any.
  LogPictureBufferUsage();

  // There shouldn't be any picture buffer.
  CHECK(picture_buffers_.empty());

  // In addition to what the decoder needs, add one picture buffer
  // for overlay weirdness, just to be safe. We may need to track
  // actual used buffers for all use cases and decide an optimal
  // number of picture buffers.
  size_t pic_buffers_required =
      accelerated_video_decoder_->GetRequiredNumOfPictures() + 1;

  // Create each picture buffer.
  for (size_t i = 0; i < pic_buffers_required; i++) {
    auto texture_wrapper = backend_->CreateOutputTextureWrapper(
        texture_selector_.get(), color_space, size);
    if (!texture_wrapper) {
      return NotifyError(
          D3DStatus::Codes::kAllocateTextureForCopyingWrapperFailed);
    }

    base::OnceCallback<void(scoped_refptr<media::D3DPictureBuffer>)>
        picture_buffer_gpu_resource_init_done_cb = base::DoNothing();

    // Creating the shared image representation needed for WebGPU zero-copy
    // interop is asynchronous. The backend keeps the picture buffer in client
    // use until GPU initialization finishes so decoding waits for it.
    picture_buffer_gpu_resource_init_done_cb =
        base::BindPostTaskToCurrentDefault(
            base::BindOnce(&D3DVideoDecoder::PictureBufferGPUResourceInitDone,
                           weak_factory_.GetWeakPtr()));

    const size_t array_slice = use_single_video_decoder_texture_ ? 0 : i;
    auto picture_buffer_result = backend_->CreateAndInitPictureBuffer(
        size, pic_buffers_required, array_slice, /*picture_index=*/i,
        use_single_video_decoder_texture_, std::move(texture_wrapper),
        decoder_configurator_.get(), texture_selector_.get(),
        decoder_task_runner_, gpu_task_runner_, get_helper_cb_,
        media_log_.get(), std::move(picture_buffer_gpu_resource_init_done_cb));
    if (!picture_buffer_result.has_value()) {
      return NotifyError(std::move(picture_buffer_result).error().AddHere());
    }

    picture_buffers_.push_back(std::move(picture_buffer_result).value());
  }

  D3DStatus result =
      d3d_video_decoder_wrapper_->SetPictureBuffers(picture_buffers_);
  if (!result.is_ok()) {
    return NotifyError(std::move(result).AddHere());
  }
}

D3DPictureBuffer* D3DVideoDecoder::GetPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& buffer : picture_buffers_) {
    if (!buffer->in_client_use() && !buffer->in_picture_use()) {
      buffer->timestamp_ = current_timestamp_;
      return buffer.get();
    }
  }

  return nullptr;
}

void D3DVideoDecoder::UpdateTimestamp(D3DPictureBuffer* picture_buffer) {
  // A picture is being reused with a different timestamp; since we've already
  // generated a VideoFrame from the previous picture buffer, we can just stamp
  // the new timestamp directly onto the buffer.
  picture_buffer->timestamp_ = current_timestamp_;
}

bool D3DVideoDecoder::OutputResult(const CodecPicture* picture,
                                   D3DPictureBuffer* picture_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(texture_selector_);
  TRACE_EVENT0("gpu", "D3DVideoDecoder::OutputResult");

  D3DStatus result = backend_->WaitForDecodeComplete(picture_buffer);
  if (!result.is_ok()) {
    NotifyError(std::move(result).AddHere());
    return false;
  }
  picture_buffer->add_client_use();

  // Note: The pixel format doesn't matter.
  gfx::Rect visible_rect = picture->visible_rect();
  if (visible_rect.IsEmpty()) {
    visible_rect = config_.visible_rect();
  }

  gfx::Size natural_size = config_.aspect_ratio().GetNaturalSize(visible_rect);
  base::TimeDelta timestamp = picture_buffer->timestamp_;

  scoped_refptr<gpu::ClientSharedImage> shared_image;
  result = picture_buffer->ProcessTexture(shared_image);
  if (!result.is_ok()) {
    NotifyError(std::move(result).AddHere());
    return false;
  }

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapSharedImage(
      texture_selector_->PixelFormat(), shared_image,
      shared_image->creation_sync_token(), VideoFrame::ReleaseMailboxCB(),
      visible_rect, natural_size, timestamp);

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
      base::BindOnce(&D3DVideoDecoder::ReceivePictureBufferFromClient,
                     weak_factory_.GetWeakPtr(),
                     scoped_refptr<D3DPictureBuffer>(picture_buffer)));
  frame->SetReleaseMailboxCB(
      base::BindOnce(release_mailbox_cb_, std::move(wait_complete_cb)));
  frame->metadata().power_efficient = true;

  if (shared_image->color_space().IsHDR()) {
    // Some streams may have varying metadata, so bitstream metadata should be
    // preferred over metadata provide by the configuration.
    gfx::HDRMetadata hdr_metadata = picture->dynamic_hdr_metadata();
    if (hdr_metadata.IsEmpty()) {
      hdr_metadata = config_.hdr_metadata();
    }
    frame->set_hdr_metadata(hdr_metadata);
  }

  frame->metadata().is_webgpu_compatible =
      !(gpu_workarounds_.disable_sharing_nv12_from_d3d11_to_d3d12 &&
        texture_selector_->OutputSharedImageFormat() ==
            viz::MultiPlaneFormat::kNV12) &&
      use_shared_handle_;

  output_cb_.Run(frame);
  return true;
}

D3DVideoDecoderWrapper* D3DVideoDecoder::GetWrapper() {
  return d3d_video_decoder_wrapper_.get();
}

bool D3DVideoDecoder::SubmitBitstreamBufferForTesting(  // IN-TEST
    base::span<const uint8_t> bitstream) {
  CHECK_IS_TEST();
  CHECK(d3d_video_decoder_wrapper_);
  ScopedSequenceD3DInputBuffer& buffer =
      d3d_video_decoder_wrapper_->GetBitstreamBuffer(bitstream.size());
  return buffer.Write(bitstream) == bitstream.size() &&
         d3d_video_decoder_wrapper_->SubmitSlice();
}

void D3DVideoDecoder::NotifyError(D3DStatus reason,
                                  DecoderStatus::Codes opt_decoder_code) {
  TRACE_EVENT0("gpu", "D3DVideoDecoder::NotifyError");

  if (!reason.is_ok() && !reason.message().empty()) {
    MEDIA_LOG(ERROR, media_log_) << "D3DVideoDecoder: " << reason.message();
  }

  PostDecoderStatus(
      DecoderStatus(opt_decoder_code).AddCause(std::move(reason)));
}

void D3DVideoDecoder::PostDecoderStatus(DecoderStatus status) {
  TRACE_EVENT0("gpu", "D3DVideoDecoder::PostDecoderStatus");

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

void D3DVideoDecoder::MeasurePictureBufferUsage() {
  // Count the total number of buffers that are currently unused by either the
  // client or the decoder.  These are buffers that we didn't need to allocate.
  int unused_buffers = 0;
  for (const auto& buffer : picture_buffers_) {
    if (!buffer->in_client_use() && !buffer->in_picture_use()) {
      unused_buffers++;
    }
  }

  if (!min_unused_buffers_ || unused_buffers < *min_unused_buffers_) {
    min_unused_buffers_ = unused_buffers;
  }
}

void D3DVideoDecoder::LogPictureBufferUsage() {
  if (!min_unused_buffers_) {
    return;
  }

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

// static
std::vector<SupportedVideoDecoderConfig>
D3DVideoDecoder::GetSupportedVideoDecoderConfigs(
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    GetD3DDeviceCB get_d3d_device_cb) {
  return CreateVideoDecoderBackend()->GetSupportedVideoDecoderConfigs(
      gpu_workarounds, std::move(get_d3d_device_cb));
}

}  // namespace media
