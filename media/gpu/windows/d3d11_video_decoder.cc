// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder.h"

#include <d3d11_4.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/win/hresult_status_helper.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_video_context_wrapper.h"
#include "media/gpu/windows/d3d11_video_decoder_impl.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "media/media_buildflags.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/hdr_metadata_helper_win.h"

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
  scoped_refptr<CommandBufferHelper> helper;

 private:
  ~CommandBufferHelperHolder() = default;
  friend class base::RefCountedDeleteOnSequence<CommandBufferHelperHolder>;
  friend class base::DeleteHelper<CommandBufferHelperHolder>;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferHelperHolder);
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
    D3D11VideoDecoder::GetD3D11DeviceCB get_d3d11_device_cb,
    SupportedConfigs supported_configs,
    bool is_hdr_supported) {
  // We create |impl_| on the wrong thread, but we never use it here.
  // Note that the output callback will hop to our thread, post the video
  // frame, and along with a callback that will hop back to the impl thread
  // when it's released.
  // Note that we WrapUnique<VideoDecoder> rather than D3D11VideoDecoder to make
  // this castable; the deleters have to match.
  std::unique_ptr<MediaLog> cloned_media_log = media_log->Clone();
  auto get_helper_cb =
      base::BindRepeating(CreateCommandBufferHelper, std::move(get_stub_cb),
                          scoped_refptr<CommandBufferHelperHolder>(
                              new CommandBufferHelperHolder(gpu_task_runner)));
  return base::WrapUnique<VideoDecoder>(new D3D11VideoDecoder(
      gpu_task_runner, std::move(media_log), gpu_preferences, gpu_workarounds,
      base::SequenceBound<D3D11VideoDecoderImpl>(
          gpu_task_runner, std::move(cloned_media_log), get_helper_cb),
      get_helper_cb, std::move(get_d3d11_device_cb),
      std::move(supported_configs), is_hdr_supported));
}

D3D11VideoDecoder::D3D11VideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::SequenceBound<D3D11VideoDecoderImpl> impl,
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb,
    GetD3D11DeviceCB get_d3d11_device_cb,
    SupportedConfigs supported_configs,
    bool is_hdr_supported)
    : media_log_(std::move(media_log)),
      impl_(std::move(impl)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      decoder_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      already_initialized_(false),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      get_d3d11_device_cb_(std::move(get_d3d11_device_cb)),
      get_helper_cb_(std::move(get_helper_cb)),
      supported_configs_(std::move(supported_configs)),
      is_hdr_supported_(is_hdr_supported) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_log_);
}

D3D11VideoDecoder::~D3D11VideoDecoder() {
  // Post destruction to the main thread.  When this executes, it will also
  // cancel pending callbacks into |impl_| via |impl_weak_|.  Callbacks out
  // from |impl_| will be cancelled by |weak_factory_| when we return.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  impl_.Reset();

  // Explicitly destroy the decoder, since it can reference picture buffers.
  accelerated_video_decoder_.reset();

  if (already_initialized_)
    AddLifetimeProgressionStage(D3D11LifetimeProgression::kPlaybackSucceeded);
}

std::string D3D11VideoDecoder::GetDisplayName() const {
  return "D3D11VideoDecoder";
}

HRESULT D3D11VideoDecoder::InitializeAcceleratedDecoder(
    const VideoDecoderConfig& config,
    ComD3D11VideoDecoder video_decoder) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::InitializeAcceleratedDecoder");
  // If we got an 11.1 D3D11 Device, we can use a |ID3D11VideoContext1|,
  // otherwise we have to make sure we only use a |ID3D11VideoContext|.
  HRESULT hr;

  // |device_context_| is the primary display context, but currently
  // we share it for decoding purposes.
  auto video_context = VideoContextWrapper::CreateWrapper(usable_feature_level_,
                                                          device_context_, &hr);

  if (!SUCCEEDED(hr))
    return hr;

  profile_ = config.profile();
  if (config.codec() == kCodecVP9) {
    accelerated_video_decoder_ = std::make_unique<VP9Decoder>(
        std::make_unique<D3D11VP9Accelerator>(
            this, media_log_.get(), video_device_, std::move(video_context)),
        profile_, config.color_space_info());
  } else if (config.codec() == kCodecH264) {
    accelerated_video_decoder_ = std::make_unique<H264Decoder>(
        std::make_unique<D3D11H264Accelerator>(
            this, media_log_.get(), video_device_, std::move(video_context)),
        profile_, config.color_space_info());
  } else {
    return E_FAIL;
  }

  // Provide the initial video decoder object.
  DCHECK(set_accelerator_decoder_cb_);
  set_accelerator_decoder_cb_.Run(std::move(video_decoder));

  return hr;
}

ErrorOr<std::tuple<ComD3D11VideoDecoder>>
D3D11VideoDecoder::CreateD3D11Decoder() {
  HRESULT hr;

  // TODO: supported check?

  decoder_configurator_ = D3D11DecoderConfigurator::Create(
      gpu_preferences_, gpu_workarounds_, config_, media_log_.get());
  if (!decoder_configurator_)
    return StatusCode::kDecoderUnsupportedProfile;

  if (!decoder_configurator_->SupportsDevice(video_device_))
    return StatusCode::kDecoderUnsupportedCodec;

  FormatSupportChecker format_checker(device_);
  if (!format_checker.Initialize()) {
    // Don't fail; it'll just return no support a lot.
    MEDIA_LOG(WARNING, media_log_)
        << "Could not create format checker, continuing";
  }

  // Use IsHDRSupported to guess whether the compositor can output HDR textures.
  // See TextureSelector for notes about why the decoder should not care.
  texture_selector_ = TextureSelector::Create(
      gpu_preferences_, gpu_workarounds_,
      decoder_configurator_->TextureFormat(),
      is_hdr_supported_ ? TextureSelector::HDRMode::kSDROrHDR
                        : TextureSelector::HDRMode::kSDROnly,
      &format_checker, media_log_.get());
  if (!texture_selector_)
    return StatusCode::kCreateTextureSelectorFailed;

  UINT config_count = 0;
  hr = video_device_->GetVideoDecoderConfigCount(
      decoder_configurator_->DecoderDescriptor(), &config_count);
  if (FAILED(hr)) {
    return Status(StatusCode::kGetDecoderConfigCountFailed)
        .AddCause(HresultToStatus(hr));
  }

  if (config_count == 0)
    return Status(StatusCode::kGetDecoderConfigCountFailed);

  D3D11_VIDEO_DECODER_CONFIG dec_config = {};
  bool found = false;

  for (UINT i = 0; i < config_count; i++) {
    hr = video_device_->GetVideoDecoderConfig(
        decoder_configurator_->DecoderDescriptor(), i, &dec_config);
    if (FAILED(hr)) {
      return Status(StatusCode::kGetDecoderConfigFailed)
          .AddCause(HresultToStatus(hr));
    }

    if (config_.codec() == kCodecVP9 && dec_config.ConfigBitstreamRaw == 1) {
      // DXVA VP9 specification mentions ConfigBitstreamRaw "shall be 1".
      found = true;
      break;
    }

    if (config_.codec() == kCodecH264 && dec_config.ConfigBitstreamRaw == 2) {
      // ConfigBitstreamRaw == 2 means the decoder uses DXVA_Slice_H264_Short.
      found = true;
      break;
    }
  }
  if (!found)
    return StatusCode::kDecoderUnsupportedConfig;

  // Prefer whatever the config tells us about whether to use one Texture2D with
  // multiple array slices, or multiple Texture2Ds with one slice each.  If bit
  // 14 is clear, then it's the former, else it's the latter.
  //
  // Let the workaround override array texture mode, if enabled.
  // TODO(crbug.com/971952): Ignore |use_single_video_decoder_texture_| here,
  // since it might be the case that it's not actually the right fix.  Instead,
  // We use this workaround to force a copy later.  The workaround will be
  // renamed if this turns out to fix the issue, but we might need to merge back
  // and smaller changes are better.
  //
  // For more information, please see:
  // https://download.microsoft.com/download/9/2/A/92A4E198-67E0-4ABD-9DB7-635D711C2752/DXVA_VPx.pdf
  // https://download.microsoft.com/download/5/f/c/5fc4ec5c-bd8c-4624-8034-319c1bab7671/DXVA_H264.pdf
  use_single_video_decoder_texture_ =
      !!(dec_config.ConfigDecoderSpecific & (1 << 14));
  if (use_single_video_decoder_texture_)
    MEDIA_LOG(INFO, media_log_) << "D3D11VideoDecoder is using single textures";
  else
    MEDIA_LOG(INFO, media_log_) << "D3D11VideoDecoder is using array texture";

  Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder;
  hr = video_device_->CreateVideoDecoder(
      decoder_configurator_->DecoderDescriptor(), &dec_config, &video_decoder);

  if (!video_decoder.Get())
    return Status(StatusCode::kDecoderCreationFailed);

  if (FAILED(hr)) {
    return Status(StatusCode::kDecoderCreationFailed)
        .AddCause(HresultToStatus(hr));
  }

  return {std::move(video_decoder)};
}

void D3D11VideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   bool low_delay,
                                   CdmContext* /* cdm_context */,
                                   InitCB init_cb,
                                   const OutputCB& output_cb,
                                   const WaitingCB& /* waiting_cb */) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::Initialize");
  if (already_initialized_)
    AddLifetimeProgressionStage(D3D11LifetimeProgression::kPlaybackSucceeded);
  AddLifetimeProgressionStage(D3D11LifetimeProgression::kInitializeStarted);

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

  if (!is_supported) {
    NotifyError("D3D11VideoDecoder does not support this config");
    return;
  }

  if (config.is_encrypted()) {
    NotifyError("D3D11VideoDecoder does not support encrypted stream");
    return;
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
  device_ = get_d3d11_device_cb_.Run();
  if (!device_) {
    // This happens if, for example, if chrome is configured to use
    // D3D9 for ANGLE.
    NotifyError("ANGLE did not provide D3D11 device");
    return;
  }

  if (!GetD3D11FeatureLevel(device_, gpu_workarounds_,
                            &usable_feature_level_)) {
    NotifyError("D3D11 feature level not supported");
    return;
  }

  device_->GetImmediateContext(&device_context_);

  HRESULT hr;

  // TODO(liberato): Handle cleanup better.  Also consider being less chatty in
  // the logs, since this will fall back.

  // TODO(liberato): dxva does this.  don't know if we need to.
  if (!base::FeatureList::IsEnabled(kD3D11VideoDecoderSkipMultithreaded)) {
    ComD3D11Multithread multi_threaded;
    hr = device_->QueryInterface(IID_PPV_ARGS(&multi_threaded));
    if (FAILED(hr)) {
      NotifyError(Status(StatusCode::kQueryID3D11MultithreadFailed)
                      .AddCause(HresultToStatus(hr)));
      return;
    }
    // TODO(liberato): This is a hack, since the unittest returns
    // success without providing |multi_threaded|.
    if (multi_threaded)
      multi_threaded->SetMultithreadProtected(TRUE);
  }

  hr = device_.As(&video_device_);
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get video device");
    return;
  }

  auto video_decoder_or_error = CreateD3D11Decoder();
  if (video_decoder_or_error.has_error()) {
    NotifyError(video_decoder_or_error.error());
    return;
  }

  hr = InitializeAcceleratedDecoder(
      config, std::move(std::get<0>(video_decoder_or_error.value())));

  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get device context");
    return;
  }

  // At this point, playback is supported so add a line in the media log to help
  // us figure that out.
  MEDIA_LOG(INFO, media_log_) << "Video is supported by D3D11VideoDecoder";

  if (base::FeatureList::IsEnabled(kD3D11PrintCodecOnCrash)) {
    static base::debug::CrashKeyString* codec_name =
        base::debug::AllocateCrashKeyString("d3d11_playback_video_codec",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(codec_name,
                                   config.GetHumanReadableCodecName());
  }

  auto impl_init_cb = base::BindOnce(&D3D11VideoDecoder::OnGpuInitComplete,
                                     weak_factory_.GetWeakPtr());

  auto get_picture_buffer_cb =
      base::BindRepeating(&D3D11VideoDecoder::ReceivePictureBufferFromClient,
                          weak_factory_.GetWeakPtr());

  AddLifetimeProgressionStage(D3D11LifetimeProgression::kInitializeSucceeded);

  // Initialize the gpu side.  It would be nice if we could ask SB<> to elide
  // the post if we're already on that thread, but it can't.
  // Bind our own init / output cb that hop to this thread, so we don't call
  // the originals on some other thread.
  // Important but subtle note: base::Bind will copy |config_| since it's a
  // const ref.
  impl_.Post(FROM_HERE, &D3D11VideoDecoderImpl::Initialize,
             BindToCurrentLoop(std::move(impl_init_cb)));
}

void D3D11VideoDecoder::AddLifetimeProgressionStage(
    D3D11LifetimeProgression stage) {
  already_initialized_ =
      (stage == D3D11LifetimeProgression::kInitializeSucceeded);
  const std::string uma_name("Media.D3D11.DecoderLifetimeProgression");
  base::UmaHistogramEnumeration(uma_name, stage);
}

void D3D11VideoDecoder::ReceivePictureBufferFromClient(
    scoped_refptr<D3D11PictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::ReceivePictureBufferFromClient");

  // We may decode into this buffer again.
  // Note that |buffer| might no longer be in |picture_buffers_| if we've
  // replaced them.  That's okay.
  buffer->set_in_client_use(false);

  // Also re-start decoding in case it was waiting for more pictures.
  DoDecode();
}

void D3D11VideoDecoder::OnGpuInitComplete(
    bool success,
    D3D11VideoDecoderImpl::ReleaseMailboxCB release_mailbox_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::OnGpuInitComplete");

  if (!init_cb_) {
    // We already failed, so just do nothing.
    DCHECK_EQ(state_, State::kError);
    return;
  }

  DCHECK_EQ(state_, State::kInitializing);

  if (!success) {
    NotifyError("Gpu init failed");
    return;
  }

  release_mailbox_cb_ = std::move(release_mailbox_cb);

  state_ = State::kRunning;
  std::move(init_cb_).Run(OkStatus());
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
    std::move(decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  input_buffer_queue_.push_back(
      std::make_pair(std::move(buffer), std::move(decode_cb)));

  // Post, since we're not supposed to call back before this returns.  It
  // probably doesn't matter since we're in the gpu process anyway.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
        NotifyError("Flush failed");
        return;
      }
      // Pictures out output synchronously during Flush.  Signal the decode
      // cb now.
      std::move(current_decode_cb_).Run(DecodeStatus::OK);
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
      std::move(current_decode_cb_).Run(DecodeStatus::OK);
      break;
    } else if (result == media::AcceleratedVideoDecoder::kRanOutOfSurfaces) {
      // At this point, we know the picture size.
      // If we haven't allocated picture buffers yet, then allocate some now.
      // Otherwise, stop here.  We'll restart when a picture comes back.
      if (picture_buffers_.size())
        return;
      CreatePictureBuffers();
    } else if (result == media::AcceleratedVideoDecoder::kConfigChange) {
      // TODO(liberato): I think we support this now, as long as it's the same
      // decoder.  Should update |config_| though.
      if (profile_ != accelerated_video_decoder_->GetProfile()) {
        // TODO(crbug.com/1022246): Handle profile change.
        LOG(ERROR) << "Profile change is not supported";
        NotifyError("Profile change is not supported");
        return;
      }
      // Before the first frame, we get a config change that we should ignore.
      // We only want to take action if this is a mid-stream config change.  We
      // could wait until now to allocate the first D3D11VideoDecoder, but we
      // don't, so that init can fail rather than decoding if there's a problem
      // creating it.  If there's a config change at the start of the stream,
      // then this might not work.
      if (!picture_buffers_.size())
        continue;

      // Update the config.
      const auto new_coded_size = accelerated_video_decoder_->GetPicSize();
      config_.set_coded_size(new_coded_size);
      auto video_decoder_or_error = CreateD3D11Decoder();
      if (video_decoder_or_error.has_error()) {
        NotifyError(video_decoder_or_error.error());
        return;
      }
      DCHECK(set_accelerator_decoder_cb_);
      set_accelerator_decoder_cb_.Run(
          std::move(std::get<0>(video_decoder_or_error.value())));
      picture_buffers_.clear();
    } else if (result == media::AcceleratedVideoDecoder::kTryAgain) {
      LOG(ERROR) << "Try again is not supported";
      NotifyError("Try again is not supported");
      return;
    } else {
      LOG(ERROR) << "VDA Error " << result;
      NotifyError("Accelerated decode failed");
      return;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitializing);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::Reset");

  current_buffer_ = nullptr;
  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::ABORTED);

  for (auto& queue_pair : input_buffer_queue_)
    std::move(queue_pair.second).Run(DecodeStatus::ABORTED);
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

  gl::HDRMetadata stream_metadata;
  if (config_.hdr_metadata())
    stream_metadata = *config_.hdr_metadata();
  // else leave |stream_metadata| default-initialized.  We might use it anyway.

  base::Optional<DXGI_HDR_METADATA_HDR10> display_metadata;
  if (decoder_configurator_->TextureFormat() == DXGI_FORMAT_P010) {
    // For HDR formats, try to get the display metadata.  This may fail, which
    // is okay.  We'll just skip sending the metadata.
    gl::HDRMetadataHelperWin hdr_metadata_helper(device_);
    display_metadata = hdr_metadata_helper.GetDisplayMetadata();
  }

  // Drop any old pictures.
  for (auto& buffer : picture_buffers_)
    DCHECK(!buffer->in_picture_use());
  picture_buffers_.clear();

  ComD3D11Texture2D in_texture;

  // Create each picture buffer.
  for (size_t i = 0; i < D3D11DecoderConfigurator::BUFFER_COUNT; i++) {
    // Create an input texture / texture array if we haven't already.
    if (!in_texture) {
      auto result = decoder_configurator_->CreateOutputTexture(
          device_, size,
          use_single_video_decoder_texture_
              ? 1
              : D3D11DecoderConfigurator::BUFFER_COUNT);
      if (result.has_value()) {
        in_texture = std::move(result.value());
      } else {
        NotifyError(std::move(result.error()).AddHere());
        return;
      }
    }

    DCHECK(!!in_texture);

    auto tex_wrapper = texture_selector_->CreateTextureWrapper(
        device_, video_device_, device_context_, size);
    if (!tex_wrapper) {
      NotifyError(StatusCode::kAllocateTextureForCopyingWrapperFailed);
      return;
    }

    const size_t array_slice = use_single_video_decoder_texture_ ? 0 : i;
    picture_buffers_.push_back(
        new D3D11PictureBuffer(decoder_task_runner_, in_texture, array_slice,
                               std::move(tex_wrapper), size, i /* level */));
    Status result = picture_buffers_[i]->Init(
        gpu_task_runner_, get_helper_cb_, video_device_,
        decoder_configurator_->DecoderGuid(), media_log_->Clone());
    if (!result.is_ok()) {
      NotifyError(std::move(result).AddHere());
      return;
    }

    // If we're using one texture per buffer, rather than an array, then clear
    // the ref to it so that we allocate a new one above.
    if (use_single_video_decoder_texture_)
      in_texture = nullptr;

    // If we have display metadata, then tell the processor.  Note that the
    // order of these calls is important, and we must set the display metadata
    // if we set the stream metadata, else it can crash on some AMD cards.
    if (display_metadata) {
      if (config_.hdr_metadata() ||
          gpu_workarounds_.use_empty_video_hdr_metadata) {
        // It's okay if this has an empty-initialized metadata.
        picture_buffers_[i]->texture_wrapper()->SetStreamHDRMetadata(
            stream_metadata);
      }
      picture_buffers_[i]->texture_wrapper()->SetDisplayHDRMetadata(
          *display_metadata);
    }
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

bool D3D11VideoDecoder::OutputResult(const CodecPicture* picture,
                                     D3D11PictureBuffer* picture_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(texture_selector_);
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::OutputResult");

  picture_buffer->set_in_client_use(true);

  // Note: The pixel format doesn't matter.
  gfx::Rect visible_rect = picture->visible_rect();
  if (visible_rect.IsEmpty())
    visible_rect = config_.visible_rect();

  // TODO(https://crbug.com/843150): Use aspect ratio from decoder (SPS) if
  // stream metadata doesn't overrride it.
  double pixel_aspect_ratio = config_.GetPixelAspectRatio();

  base::TimeDelta timestamp = picture_buffer->timestamp_;

  MailboxHolderArray mailbox_holders;
  gfx::ColorSpace output_color_space;
  Status result = picture_buffer->ProcessTexture(
      picture->get_colorspace().ToGfxColorSpace(), &mailbox_holders,
      &output_color_space);
  if (!result.is_ok()) {
    NotifyError(std::move(result).AddHere());
    return false;
  }

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      texture_selector_->PixelFormat(), mailbox_holders,
      VideoFrame::ReleaseMailboxCB(), picture_buffer->size(), visible_rect,
      GetNaturalSize(visible_rect, pixel_aspect_ratio), timestamp);

  if (!frame) {
    // This can happen if, somehow, we get an unsupported combination of
    // pixel format, etc.
    NotifyError(StatusCode::kDecoderVideoFrameConstructionFailed);
    return false;
  }

  // Remember that this will likely thread-hop to the GPU main thread.  Note
  // that |picture_buffer| will delete on sequence, so it's okay even if
  // |wait_complete_cb| doesn't ever run.
  auto wait_complete_cb = BindToCurrentLoop(
      base::BindOnce(&D3D11VideoDecoder::ReceivePictureBufferFromClient,
                     weak_factory_.GetWeakPtr(),
                     scoped_refptr<D3D11PictureBuffer>(picture_buffer)));
  frame->SetReleaseMailboxCB(
      base::BindOnce(release_mailbox_cb_, std::move(wait_complete_cb)));

  frame->metadata()->power_efficient = true;
  // For NV12, overlay is allowed by default. If the decoder is going to support
  // non-NV12 textures, then this may have to be conditionally set. Also note
  // that ALLOW_OVERLAY is required for encrypted video path.
  //
  // Since all of our picture buffers allow overlay, we just use the finch
  // feature.  However, we may choose to set ALLOW_OVERLAY to false even if
  // the finch flag is enabled.  We may not choose to set ALLOW_OVERLAY if the
  // flag is off, however.
  //
  // Also note that, since we end up binding textures with GLImageDXGI, it's
  // probably okay just to allow overlay always, and let the swap chain
  // presenter decide if it wants to.
  const bool allow_overlay =
      base::FeatureList::IsEnabled(kD3D11VideoDecoderAllowOverlay);
  frame->metadata()->allow_overlay = allow_overlay;

  frame->set_color_space(output_color_space);
  frame->set_hdr_metadata(config_.hdr_metadata());
  output_cb_.Run(frame);
  return true;
}

void D3D11VideoDecoder::SetDecoderCB(const SetAcceleratorDecoderCB& cb) {
  set_accelerator_decoder_cb_ = cb;
}

// TODO(tmathmeyer): Please don't add new uses of this overload.
void D3D11VideoDecoder::NotifyError(const char* reason) {
  NotifyError(Status(StatusCode::kDecoderInitializeNeverCompleted, reason));
}

void D3D11VideoDecoder::NotifyError(const Status& reason) {
  TRACE_EVENT0("gpu", "D3D11VideoDecoder::NotifyError");
  state_ = State::kError;

  // TODO(tmathmeyer) - Remove this after plumbing Status through the
  // decode_cb and input_buffer_queue cb's.
  MEDIA_LOG(ERROR, media_log_)
      << "D3D11VideoDecoder error: " << std::hex << reason.code();

  if (init_cb_)
    std::move(init_cb_).Run(reason);

  current_buffer_ = nullptr;
  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::DECODE_ERROR);

  for (auto& queue_pair : input_buffer_queue_)
    std::move(queue_pair.second).Run(DecodeStatus::DECODE_ERROR);
  input_buffer_queue_.clear();
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

  // TODO(tmathmeyer) should we log this to UMA?
  if (gpu_workarounds.limit_d3d11_video_decoder_to_11_0 &&
      !base::FeatureList::IsEnabled(kD3D11VideoDecoderIgnoreWorkarounds)) {
    *feature_level = D3D_FEATURE_LEVEL_11_0;
  }

  return true;
}

// static
std::vector<SupportedVideoDecoderConfig>
D3D11VideoDecoder::GetSupportedVideoDecoderConfigs(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    GetD3D11DeviceCB get_d3d11_device_cb) {
  const std::string uma_name("Media.D3D11.WasVideoSupported");

  // This workaround accounts for almost half of all startup results, and it's
  // unclear that it's relevant here.  If it's off, or if we're allowed to copy
  // pictures in case binding isn't allowed, then proceed with init.
  // NOTE: experimentation showed that, yes, it does actually matter.
  if (!base::FeatureList::IsEnabled(kD3D11VideoDecoderCopyPictures)) {
    // Must allow zero-copy of nv12 textures.
    if (!gpu_preferences.enable_zero_copy_dxgi_video) {
      UMA_HISTOGRAM_ENUMERATION(uma_name,
                                NotSupportedReason::kZeroCopyNv12Required);
      return {};
    }

    if (gpu_workarounds.disable_dxgi_zero_copy_video) {
      UMA_HISTOGRAM_ENUMERATION(uma_name,
                                NotSupportedReason::kZeroCopyVideoRequired);
      return {};
    }
  }

  if (!base::FeatureList::IsEnabled(kD3D11VideoDecoderIgnoreWorkarounds)) {
    // Allow all of d3d11 to be turned off by workaround.
    if (gpu_workarounds.disable_d3d11_video_decoder) {
      UMA_HISTOGRAM_ENUMERATION(uma_name, NotSupportedReason::kOffByWorkaround);
      return {};
    }
  }

  // Remember that this might query the angle device, so this won't work if
  // we're not on the GPU main thread.  Also remember that devices are thread
  // safe (contexts are not), so we could use the angle device from any thread
  // as long as we're not calling into possible not-thread-safe things to get
  // it.  I.e., if this cached it, then it'd be fine.  It's up to our caller
  // to guarantee that, though.
  //
  // Note also that, currently, we are called from the GPU main thread only.
  auto d3d11_device = get_d3d11_device_cb.Run();
  if (!d3d11_device) {
    UMA_HISTOGRAM_ENUMERATION(uma_name,
                              NotSupportedReason::kCouldNotGetD3D11Device);
    return {};
  }

  D3D_FEATURE_LEVEL usable_feature_level;
  if (!GetD3D11FeatureLevel(d3d11_device, gpu_workarounds,
                            &usable_feature_level)) {
    UMA_HISTOGRAM_ENUMERATION(
        uma_name, NotSupportedReason::kInsufficientD3D11FeatureLevel);
    return {};
  }

  const auto supported_resolutions =
      GetSupportedD3D11VideoDecoderResolutions(d3d11_device, gpu_workarounds);

  std::vector<SupportedVideoDecoderConfig> configs;
  for (const auto& kv : supported_resolutions) {
    const auto profile = kv.first;
    if (profile == VP9PROFILE_PROFILE2 &&
        !base::FeatureList::IsEnabled(kD3D11VideoDecoderVP9Profile2)) {
      continue;
    }

    // TODO(liberato): Add VP8 and AV1 support to D3D11VideoDecoder.
    if (profile == VP8PROFILE_ANY ||
        (profile >= AV1PROFILE_MIN && profile <= AV1PROFILE_MAX)) {
      continue;
    }

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

  // TODO(liberato): Should we separate out h264 and vp9?
  UMA_HISTOGRAM_ENUMERATION(uma_name, NotSupportedReason::kVideoIsSupported);

  return configs;
}

}  // namespace media
