// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder.h"

#include <d3d11_4.h>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_video_decoder_impl.h"
#include "ui/gl/gl_angle_util_win.h"

namespace media {

namespace {

#define INRANGE(_profile, codecname) \
  (_profile >= codecname##PROFILE_MIN && _profile <= codecname##PROFILE_MAX)

bool IsVP9(const VideoDecoderConfig& config) {
  return INRANGE(config.profile(), VP9);
}

bool IsH264(const VideoDecoderConfig& config) {
  return INRANGE(config.profile(), H264);
}

bool IsUnsupportedVP9Profile(const VideoDecoderConfig& config) {
  return config.profile() == VP9PROFILE_PROFILE1 ||
         config.profile() == VP9PROFILE_PROFILE3;
}

#undef INRANGE

}  // namespace

std::unique_ptr<VideoDecoder> D3D11VideoDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb) {
  // We create |impl_| on the wrong thread, but we never use it here.
  // Note that the output callback will hop to our thread, post the video
  // frame, and along with a callback that will hop back to the impl thread
  // when it's released.
  // Note that we WrapUnique<VideoDecoder> rather than D3D11VideoDecoder to make
  // this castable; the deleters have to match.
  std::unique_ptr<MediaLog> cloned_media_log = media_log->Clone();
  return base::WrapUnique<VideoDecoder>(
      new D3D11VideoDecoder(std::move(gpu_task_runner), std::move(media_log),
                            gpu_preferences, gpu_workarounds,
                            std::make_unique<D3D11VideoDecoderImpl>(
                                std::move(cloned_media_log), get_stub_cb),
                            get_stub_cb));
}

D3D11VideoDecoder::D3D11VideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    std::unique_ptr<D3D11VideoDecoderImpl> impl,
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb)
    : media_log_(std::move(media_log)),
      impl_(std::move(impl)),
      impl_task_runner_(std::move(gpu_task_runner)),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      create_device_func_(base::BindRepeating(D3D11CreateDevice)),
      get_stub_cb_(get_stub_cb),
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  impl_weak_ = impl_->GetWeakPtr();
}

D3D11VideoDecoder::~D3D11VideoDecoder() {
  // Post destruction to the main thread.  When this executes, it will also
  // cancel pending callbacks into |impl_| via |impl_weak_|.  Callbacks out
  // from |impl_| will be cancelled by |weak_factory_| when we return.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (impl_task_runner_->RunsTasksInCurrentSequence())
    impl_.reset();
  else
    impl_task_runner_->DeleteSoon(FROM_HERE, std::move(impl_));

  // Explicitly destroy the decoder, since it can reference picture buffers.
  accelerated_video_decoder_.reset();
}

std::string D3D11VideoDecoder::GetDisplayName() const {
  return "D3D11VideoDecoder";
}

void D3D11VideoDecoder::InitializeAcceleratedDecoder(
    const VideoDecoderConfig& config,
    CdmProxyContext* proxy_context,
    Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder) {
  if (IsVP9(config)) {
    accelerated_video_decoder_ = std::make_unique<VP9Decoder>(
        std::make_unique<D3D11VP9Accelerator>(this, media_log_.get(),
                                              proxy_context, video_decoder,
                                              video_device_, video_context_),
        config.color_space_info());
    return;
  }

  if (IsH264(config)) {
    accelerated_video_decoder_ = std::make_unique<H264Decoder>(
        std::make_unique<D3D11H264Accelerator>(this, media_log_.get(),
                                               proxy_context, video_decoder,
                                               video_device_, video_context_),
        config.color_space_info());
    return;
  }

  // No other type of config should make it this far due to earlier checks.
  NOTREACHED();
}

bool D3D11VideoDecoder::DeviceHasDecoderID(GUID decoder_guid) {
  UINT index = video_device_->GetVideoDecoderProfileCount();
  while (index-- > 0) {
    GUID profile = {};
    if (SUCCEEDED(video_device_->GetVideoDecoderProfile(index, &profile))) {
      if (profile == decoder_guid)
        return true;
    }
  }
  return false;
}

GUID D3D11VideoDecoder::GetD3D11DecoderGUID(const VideoDecoderConfig& config) {
  if (IsVP9(config) && base::FeatureList::IsEnabled(kD3D11VP9Decoder))
    // TODO(tmathmeyer) set up a finch experiment.
    return D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0;

  if (IsH264(config))
    return D3D11_DECODER_PROFILE_H264_VLD_NOFGT;

  return {};
}

void D3D11VideoDecoder::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsPotentiallySupported(config)) {
    DVLOG(3) << "D3D11 video decoder not supported for the config.";
    init_cb.Run(false);
    return;
  }

  init_cb_ = init_cb;
  output_cb_ = output_cb;
  is_encrypted_ = config.is_encrypted();

  D3D11VideoDecoderImpl::InitCB cb = base::BindOnce(
      &D3D11VideoDecoder::OnGpuInitComplete, weak_factory_.GetWeakPtr());

  D3D11VideoDecoderImpl::ReturnPictureBufferCB return_picture_buffer_cb =
      base::BindRepeating(&D3D11VideoDecoder::ReceivePictureBufferFromClient,
                          weak_factory_.GetWeakPtr());

  // Initialize the video decoder.

  // Use the ANGLE device, rather than create our own.  It would be nice if we
  // could use our own device, and run on the mojo thread, but texture sharing
  // seems to be difficult.
  // TODO(liberato): take |device_| as input.
  // TODO(liberato): On re-init, we can probably re-use the device.
  device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  if (!device_) {
    // This happens if, for example, if chrome is configured to use
    // D3D9 for ANGLE.
    NotifyError("ANGLE did not provide D3D11 device");
    return;
  }
  device_->GetImmediateContext(device_context_.ReleaseAndGetAddressOf());

  HRESULT hr;

  // TODO(liberato): Handle cleanup better.  Also consider being less chatty in
  // the logs, since this will fall back.
  hr = device_context_.CopyTo(video_context_.ReleaseAndGetAddressOf());
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get device context");
    return;
  }

  hr = device_.CopyTo(video_device_.ReleaseAndGetAddressOf());
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get video device");
    return;
  }

  GUID decoder_guid = GetD3D11DecoderGUID(config);
  if (!DeviceHasDecoderID(decoder_guid)) {
    NotifyError("Did not find a supported profile");
    return;
  }

  // TODO(liberato): dxva does this.  don't know if we need to.
  Microsoft::WRL::ComPtr<ID3D11Multithread> multi_threaded;
  hr = device_->QueryInterface(IID_PPV_ARGS(&multi_threaded));
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to query ID3D11Multithread");
    return;
  }
  multi_threaded->SetMultithreadProtected(TRUE);

  D3D11_VIDEO_DECODER_DESC desc = {};
  desc.Guid = decoder_guid;
  desc.SampleWidth = config.coded_size().width();
  desc.SampleHeight = config.coded_size().height();
  desc.OutputFormat = DXGI_FORMAT_NV12;
  UINT config_count = 0;
  hr = video_device_->GetVideoDecoderConfigCount(&desc, &config_count);
  if (FAILED(hr) || config_count == 0) {
    NotifyError("Failed to get video decoder config count");
    return;
  }

  D3D11_VIDEO_DECODER_CONFIG dec_config = {};
  bool found = false;
  for (UINT i = 0; i < config_count; i++) {
    hr = video_device_->GetVideoDecoderConfig(&desc, i, &dec_config);
    if (FAILED(hr)) {
      NotifyError("Failed to get decoder config");
      return;
    }
    if (dec_config.ConfigBitstreamRaw == 2) {
      found = true;
      break;
    }
  }
  if (!found) {
    NotifyError("Failed to find decoder config");
    return;
  }

  if (is_encrypted_)
    dec_config.guidConfigBitstreamEncryption = D3D11_DECODER_ENCRYPTION_HW_CENC;

  memcpy(&decoder_guid_, &decoder_guid, sizeof decoder_guid_);

  Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder;
  hr = video_device_->CreateVideoDecoder(
      &desc, &dec_config, video_decoder.ReleaseAndGetAddressOf());
  if (!video_decoder.Get()) {
    NotifyError("Failed to create a video decoder");
    return;
  }

  CdmProxyContext* proxy_context = nullptr;
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (cdm_context)
    proxy_context = cdm_context->GetCdmProxyContext();
#endif

  // Ensure that if we are encrypted, that we have a CDM.
  if (is_encrypted_ && !proxy_context) {
    NotifyError("Video stream is encrypted, but no cdm was found");
    return;
  }

  InitializeAcceleratedDecoder(config, proxy_context, video_decoder);

  // |cdm_context| could be null for clear playback.
  // TODO(liberato): On re-init, should this still happen?
  if (cdm_context) {
    new_key_callback_registration_ =
        cdm_context->RegisterNewKeyCB(base::BindRepeating(
            &D3D11VideoDecoder::NotifyNewKey, weak_factory_.GetWeakPtr()));
  }

  // Initialize the gpu side.  We wait until everything else is initialized,
  // since we allow it to call us back re-entrantly to reduce latency.  Note
  // that if we're not on the same thread, then we should probably post the
  // call earlier, since re-entrancy won't be an issue.
  if (impl_task_runner_->RunsTasksInCurrentSequence()) {
    impl_->Initialize(std::move(cb), std::move(return_picture_buffer_cb));
    return;
  }

  // Bind our own init / output cb that hop to this thread, so we don't call
  // the originals on some other thread.
  // Important but subtle note: base::Bind will copy |config_| since it's a
  // const ref.
  impl_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoderImpl::Initialize, impl_weak_,
                     BindToCurrentLoop(std::move(cb)),
                     BindToCurrentLoop(std::move(return_picture_buffer_cb))));
}

void D3D11VideoDecoder::ReceivePictureBufferFromClient(
    scoped_refptr<D3D11PictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We may decode into this buffer again.
  // Note that |buffer| might no longer be in |picture_buffers_| if we've
  // replaced them.  That's okay.
  buffer->set_in_client_use(false);

  // Also re-start decoding in case it was waiting for more pictures.
  DoDecode();
}

void D3D11VideoDecoder::OnGpuInitComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!init_cb_) {
    // We already failed, so just do nothing.
    return;
  }

  if (!success) {
    NotifyError("Gpu init failed");
    return;
  }

  state_ = State::kRunning;
  std::move(init_cb_).Run(true);
}

void D3D11VideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               const DecodeCB& decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kError) {
    // TODO(liberato): consider posting, though it likely doesn't matter.
    decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  input_buffer_queue_.push_back(std::make_pair(std::move(buffer), decode_cb));
  // Post, since we're not supposed to call back before this returns.  It
  // probably doesn't matter since we're in the gpu process anyway.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoder::DoDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kRunning)
    return;

  if (!current_buffer_) {
    if (input_buffer_queue_.empty()) {
      return;
    }
    current_buffer_ = std::move(input_buffer_queue_.front().first);
    current_decode_cb_ = input_buffer_queue_.front().second;
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

    accelerated_video_decoder_->SetStream(-1, current_buffer_->data(),
                                          current_buffer_->data_size(),
                                          current_buffer_->decrypt_config());
  }

  while (true) {
    // If we transition to the error state, then stop here.
    if (state_ == State::kError)
      return;

    media::AcceleratedVideoDecoder::DecodeResult result =
        accelerated_video_decoder_->Decode();
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
    } else if (result == media::AcceleratedVideoDecoder::kAllocateNewSurfaces) {
      CreatePictureBuffers();
    } else if (result == media::AcceleratedVideoDecoder::kTryAgain) {
      state_ = State::kWaitingForNewKey;
      // Note that another DoDecode() task would be posted in NotifyNewKey().
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

void D3D11VideoDecoder::Reset(const base::RepeatingClosure& closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_buffer_ = nullptr;
  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::ABORTED);

  for (auto& queue_pair : input_buffer_queue_)
    queue_pair.second.Run(DecodeStatus::ABORTED);
  input_buffer_queue_.clear();

  // TODO(liberato): how do we signal an error?
  accelerated_video_decoder_->Reset();
  closure.Run();
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(liberato): what's the minimum that we need for the decoder?
  // the VDA requests 20.
  const int num_buffers = 20;

  gfx::Size size = accelerated_video_decoder_->GetPicSize();

  // Create an array of |num_buffers| elements to back the PictureBuffers.
  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture_desc.Width = size.width();
  texture_desc.Height = size.height();
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = num_buffers;
  texture_desc.Format = DXGI_FORMAT_NV12;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
  texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  if (is_encrypted_)
    texture_desc.MiscFlags |= D3D11_RESOURCE_MISC_HW_PROTECTED;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> out_texture;
  HRESULT hr = device_->CreateTexture2D(&texture_desc, nullptr,
                                        out_texture.ReleaseAndGetAddressOf());
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to create a Texture2D for PictureBuffers");
    return;
  }

  // Drop any old pictures.
  for (auto& buffer : picture_buffers_)
    DCHECK(!buffer->in_picture_use());
  picture_buffers_.clear();

  // Create each picture buffer.
  const int textures_per_picture = 2;  // From the VDA
  for (size_t i = 0; i < num_buffers; i++) {
    picture_buffers_.push_back(
        new D3D11PictureBuffer(GL_TEXTURE_EXTERNAL_OES, size, i));
    if (!picture_buffers_[i]->Init(get_stub_cb_, video_device_, out_texture,
                                   decoder_guid_, textures_per_picture)) {
      NotifyError("Unable to allocate PictureBuffer");
      return;
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

void D3D11VideoDecoder::OutputResult(const CodecPicture* picture,
                                     D3D11PictureBuffer* picture_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  picture_buffer->set_in_client_use(true);

  // Note: The pixel format doesn't matter.
  gfx::Rect visible_rect(picture->visible_rect());
  // TODO(liberato): Pixel aspect ratio should come from the VideoDecoderConfig
  // (except when it should come from the SPS).
  // https://crbug.com/837337
  double pixel_aspect_ratio = 1.0;
  base::TimeDelta timestamp = picture_buffer->timestamp_;
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_NV12, picture_buffer->mailbox_holders(),
      VideoFrame::ReleaseMailboxCB(), picture_buffer->size(), visible_rect,
      GetNaturalSize(visible_rect, pixel_aspect_ratio), timestamp);

  // TODO(liberato): bind this to the gpu main thread.
  frame->SetReleaseMailboxCB(media::BindToCurrentLoop(
      base::BindOnce(&D3D11VideoDecoderImpl::OnMailboxReleased, impl_weak_,
                     scoped_refptr<D3D11PictureBuffer>(picture_buffer))));
  frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, true);
  // For NV12, overlay is allowed by default. If the decoder is going to support
  // non-NV12 textures, then this may have to be conditionally set. Also note
  // that ALLOW_OVERLAY is required for encrypted video path.
  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);

  if (is_encrypted_) {
    frame->metadata()->SetBoolean(VideoFrameMetadata::PROTECTED_VIDEO, true);
    frame->metadata()->SetBoolean(VideoFrameMetadata::REQUIRE_OVERLAY, true);
  }

  frame->set_color_space(picture->get_colorspace().ToGfxColorSpace());
  output_cb_.Run(frame);
}

void D3D11VideoDecoder::NotifyNewKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kWaitingForNewKey) {
    // Note that this method may be called before DoDecode() because the key
    // acquisition stack may be running independently of the media decoding
    // stack. So if this isn't in kWaitingForNewKey state no "resuming" is
    // required therefore no special action taken here.
    return;
  }

  state_ = State::kRunning;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoder::NotifyError(const char* reason) {
  state_ = State::kError;
  DLOG(ERROR) << reason;
  if (media_log_) {
    media_log_->AddEvent(media_log_->CreateStringEvent(
        MediaLogEvent::MEDIA_ERROR_LOG_ENTRY, "error", reason));
  }

  if (init_cb_)
    std::move(init_cb_).Run(false);

  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::DECODE_ERROR);

  for (auto& queue_pair : input_buffer_queue_)
    queue_pair.second.Run(DecodeStatus::DECODE_ERROR);
  input_buffer_queue_.clear();
}

void D3D11VideoDecoder::SetCreateDeviceCallbackForTesting(
    D3D11CreateDeviceCB callback) {
  create_device_func_ = std::move(callback);
}

void D3D11VideoDecoder::SetWasSupportedReason(
    D3D11VideoNotSupportedReason enum_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_ENUMERATION("Media.D3D11.WasVideoSupported", enum_value);

  const char* reason = nullptr;
  switch (enum_value) {
    case D3D11VideoNotSupportedReason::kVideoIsSupported:
      reason = "Playback is supported by D3D11VideoDecoder";
      break;
    case D3D11VideoNotSupportedReason::kInsufficientD3D11FeatureLevel:
      reason = "Insufficient D3D11 feature level";
      break;
    case D3D11VideoNotSupportedReason::kProfileNotSupported:
      reason = "Video profile is not supported by D3D11VideoDecoder";
      break;
    case D3D11VideoNotSupportedReason::kCodecNotSupported:
      reason = "H264 is required for D3D11VideoDecoder";
      break;
    case D3D11VideoNotSupportedReason::kZeroCopyNv12Required:
      reason = "Must allow zero-copy NV12 for D3D11VideoDecoder";
      break;
    case D3D11VideoNotSupportedReason::kZeroCopyVideoRequired:
      reason = "Must allow zero-copy video for D3D11VideoDecoder";
      break;
    case D3D11VideoNotSupportedReason::kEncryptedMedia:
      reason = "Encrypted media is not enabled for D3D11VideoDecoder";
      break;
  }

  DVLOG(2) << reason;
  if (media_log_) {
    media_log_->AddEvent(media_log_->CreateStringEvent(
        MediaLogEvent::MEDIA_INFO_LOG_ENTRY, "info", reason));
  }
}

bool D3D11VideoDecoder::IsPotentiallySupported(
    const VideoDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(liberato): All of this could be moved into MojoVideoDecoder, so that
  // it could run on the client side and save the IPC hop.

  if (config.is_encrypted() &&
      !base::FeatureList::IsEnabled(kD3D11EncryptedMedia)) {
    SetWasSupportedReason(D3D11VideoNotSupportedReason::kEncryptedMedia);
    return false;
  }

  // TODO(liberato): It would be nice to QueryD3D11DeviceObjectFromANGLE, but
  // we don't know what thread we're on.

  // Make sure that we support at least 11.0.
  D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_0,
  };
  HRESULT hr = create_device_func_.Run(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, ARRAYSIZE(levels),
      D3D11_SDK_VERSION, nullptr, nullptr, nullptr);

  if (FAILED(hr)) {
    SetWasSupportedReason(
        D3D11VideoNotSupportedReason::kInsufficientD3D11FeatureLevel);
    return false;
  }

  if (config.profile() == H264PROFILE_HIGH10PROFILE) {
    // H264 HIGH10 is never supported.
    SetWasSupportedReason(D3D11VideoNotSupportedReason::kProfileNotSupported);
    return false;
  }

  if (IsUnsupportedVP9Profile(config)) {
    SetWasSupportedReason(D3D11VideoNotSupportedReason::kProfileNotSupported);
    return false;
  }

  // Converts one of chromium's VideoCodecProfile options to a dxguid value.
  // If this GUID comes back empty then the profile is not supported.
  GUID decoder_GUID = GetD3D11DecoderGUID(config);

  // If we got the empty guid, fail.
  GUID empty_guid = {};
  if (decoder_GUID == empty_guid) {
    SetWasSupportedReason(D3D11VideoNotSupportedReason::kCodecNotSupported);
    return false;
  }

  // TODO(liberato): dxva checks IsHDR() in the target colorspace, but we don't
  // have the target colorspace.  It's commented as being for vpx, though, so
  // we skip it here for now.

  // Must allow zero-copy of nv12 textures.
  if (!gpu_preferences_.enable_zero_copy_dxgi_video) {
    SetWasSupportedReason(D3D11VideoNotSupportedReason::kZeroCopyNv12Required);
    return false;
  }

  if (gpu_workarounds_.disable_dxgi_zero_copy_video) {
    SetWasSupportedReason(D3D11VideoNotSupportedReason::kZeroCopyVideoRequired);
    return false;
  }

  SetWasSupportedReason(D3D11VideoNotSupportedReason::kVideoIsSupported);
  return true;
}

}  // namespace media
