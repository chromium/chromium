// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/media_codec_video_decoder.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/scoped_async_trace.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/gpu/android/android_video_surface_chooser.h"
#include "media/gpu/android/codec_allocator.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/base/android/extract_sps_and_pps.h"
#endif

namespace media {
namespace {

// Don't use MediaCodec's internal software decoders when we have more secure
// and up to date versions in the renderer process.
bool IsMediaCodecSoftwareDecodingForbidden(const VideoDecoderConfig& config) {
  return !config.is_encrypted() &&
         (config.codec() == kCodecVP8 || config.codec() == kCodecVP9);
}

bool ConfigSupported(const VideoDecoderConfig& config,
                     DeviceInfo* device_info) {
  // Don't support larger than 4k because it won't perform well on many devices.
  const auto size = config.coded_size();
  if (size.width() > 3840 || size.height() > 2160)
    return false;

  // Only use MediaCodec for VP8 or VP9 if it's likely backed by hardware or if
  // the stream is encrypted.
  const auto codec = config.codec();
  if (IsMediaCodecSoftwareDecodingForbidden(config) &&
      device_info->IsDecoderKnownUnaccelerated(codec)) {
    DVLOG(2) << "Config not supported: " << GetCodecName(codec)
             << " is not hardware accelerated";
    return false;
  }

  switch (codec) {
    case kCodecVP8:
    case kCodecVP9: {
      if ((codec == kCodecVP8 && !device_info->IsVp8DecoderAvailable()) ||
          (codec == kCodecVP9 && !device_info->IsVp9DecoderAvailable())) {
        return false;
      }

      // There's no fallback for encrypted content so we support all sizes.
      if (config.is_encrypted())
        return true;

      // Below 360p there's little to no power benefit to using MediaCodec over
      // libvpx so we prefer to fall back to that.
      if (size.width() < 480 || size.height() < 360)
        return false;

      return true;
    }
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case kCodecH264:
      return true;
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    case kCodecHEVC:
      return true;
#endif
#endif
    default:
      return false;
  }
}

void OutputBufferReleased(bool using_async_api,
                          base::RepeatingClosure pump_cb,
                          bool is_drained_or_draining) {
  // The asynchronous API doesn't need pumping upon calls to ReleaseOutputBuffer
  // unless we're draining or drained.
  if (using_async_api && !is_drained_or_draining)
    return;
  pump_cb.Run();
}

bool IsSurfaceControlEnabled(const gpu::GpuFeatureInfo& info) {
  return info.status_values[gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] ==
         gpu::kGpuFeatureStatusEnabled;
}

}  // namespace

// static
PendingDecode PendingDecode::CreateEos() {
  return {DecoderBuffer::CreateEOSBuffer(), base::DoNothing()};
}

PendingDecode::PendingDecode(scoped_refptr<DecoderBuffer> buffer,
                             VideoDecoder::DecodeCB decode_cb)
    : buffer(std::move(buffer)), decode_cb(std::move(decode_cb)) {}
PendingDecode::PendingDecode(PendingDecode&& other) = default;
PendingDecode::~PendingDecode() = default;

MediaCodecVideoDecoder::MediaCodecVideoDecoder(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    DeviceInfo* device_info,
    CodecAllocator* codec_allocator,
    std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
    AndroidOverlayMojoFactoryCB overlay_factory_cb,
    RequestOverlayInfoCB request_overlay_info_cb,
    std::unique_ptr<VideoFrameFactory> video_frame_factory)
    : codec_allocator_(codec_allocator),
      request_overlay_info_cb_(std::move(request_overlay_info_cb)),
      is_surface_control_enabled_(IsSurfaceControlEnabled(gpu_feature_info)),
      surface_chooser_helper_(
          std::move(surface_chooser),
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kForceVideoOverlays),
          base::FeatureList::IsEnabled(media::kUseAndroidOverlayAggressively),
          is_surface_control_enabled_),
      video_frame_factory_(std::move(video_frame_factory)),
      overlay_factory_cb_(std::move(overlay_factory_cb)),
      device_info_(device_info),
      enable_threaded_texture_mailboxes_(
          gpu_preferences.enable_threaded_texture_mailboxes),
      weak_factory_(this),
      codec_allocator_weak_factory_(this) {
  DVLOG(2) << __func__;
  surface_chooser_helper_.chooser()->SetClientCallbacks(
      base::Bind(&MediaCodecVideoDecoder::OnSurfaceChosen,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&MediaCodecVideoDecoder::OnSurfaceChosen,
                 weak_factory_.GetWeakPtr(), nullptr));
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  DVLOG(2) << __func__;
  TRACE_EVENT0("media", "MediaCodecVideoDecoder::~MediaCodecVideoDecoder");
  ReleaseCodec();
  codec_allocator_->StopThread(this);
}

void MediaCodecVideoDecoder::Destroy() {
  DVLOG(1) << __func__;
  TRACE_EVENT0("media", "MediaCodecVideoDecoder::Destroy");

  if (media_crypto_context_) {
    // Cancel previously registered callback (if any).
    media_crypto_context_->SetMediaCryptoReadyCB(base::NullCallback());
    if (cdm_registration_id_)
      media_crypto_context_->UnregisterPlayer(cdm_registration_id_);
    media_crypto_context_ = nullptr;
    cdm_registration_id_ = 0;
  }

  // Mojo callbacks require that they're run before destruction.
  if (reset_cb_)
    std::move(reset_cb_).Run();

  // Cancel callbacks we no longer want.
  codec_allocator_weak_factory_.InvalidateWeakPtrs();
  CancelPendingDecodes(DecodeStatus::ABORTED);
  StartDrainingCodec(DrainType::kForDestroy);
}

void MediaCodecVideoDecoder::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& /* waiting_for_decryption_key_cb */) {
  const bool first_init = !decoder_config_.IsValidConfig();
  DVLOG(1) << (first_init ? "Initializing" : "Reinitializing")
           << " MCVD with config: " << config.AsHumanReadableString()
           << ", cdm_context = " << cdm_context;

  InitCB bound_init_cb = BindToCurrentLoop(init_cb);
  if (!ConfigSupported(config, device_info_)) {
    bound_init_cb.Run(false);
    return;
  }

  // Disallow codec changes when reinitializing.
  if (!first_init && decoder_config_.codec() != config.codec()) {
    DVLOG(1) << "Codec changed: cannot reinitialize";
    bound_init_cb.Run(false);
    return;
  }
  decoder_config_ = config;

  surface_chooser_helper_.SetVideoRotation(decoder_config_.video_rotation());

  output_cb_ = output_cb;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (config.codec() == kCodecH264)
    ExtractSpsAndPps(config.extra_data(), &csd0_, &csd1_);
#endif

  // We only support setting CDM at first initialization. Even if the initial
  // config is clear, we'll still try to set CDM since we may switch to an
  // encrypted config later.
  if (first_init && cdm_context && cdm_context->GetMediaCryptoContext()) {
    DCHECK(media_crypto_.is_null());
    SetCdm(cdm_context, init_cb);
    return;
  }

  if (config.is_encrypted() && media_crypto_.is_null()) {
    DVLOG(1) << "No MediaCrypto to handle encrypted config";
    bound_init_cb.Run(false);
    return;
  }

  // Do the rest of the initialization lazily on the first decode.
  init_cb.Run(true);
}

void MediaCodecVideoDecoder::SetCdm(CdmContext* cdm_context,
                                    const InitCB& init_cb) {
  DVLOG(1) << __func__;
  DCHECK(cdm_context) << "No CDM provided";
  DCHECK(cdm_context->GetMediaCryptoContext());

  media_crypto_context_ = cdm_context->GetMediaCryptoContext();

  // Register CDM callbacks. The callbacks registered will be posted back to
  // this thread via BindToCurrentLoop.
  media_crypto_context_->SetMediaCryptoReadyCB(media::BindToCurrentLoop(
      base::Bind(&MediaCodecVideoDecoder::OnMediaCryptoReady,
                 weak_factory_.GetWeakPtr(), init_cb)));
}

void MediaCodecVideoDecoder::OnMediaCryptoReady(
    const InitCB& init_cb,
    JavaObjectPtr media_crypto,
    bool requires_secure_video_codec) {
  DVLOG(1) << __func__
           << ": requires_secure_video_codec = " << requires_secure_video_codec;

  DCHECK(state_ == State::kInitializing);
  DCHECK(media_crypto);

  if (media_crypto->is_null()) {
    media_crypto_context_->SetMediaCryptoReadyCB(
        MediaCryptoContext::MediaCryptoReadyCB());
    media_crypto_context_ = nullptr;

    if (decoder_config_.is_encrypted()) {
      LOG(ERROR) << "MediaCrypto is not available";
      EnterTerminalState(State::kError);
      init_cb.Run(false);
      return;
    }

    // MediaCrypto is not available, but the stream is clear. So we can still
    // play the current stream. But if we switch to an encrypted stream playback
    // will fail.
    init_cb.Run(true);
    return;
  }

  media_crypto_ = *media_crypto;
  requires_secure_codec_ = requires_secure_video_codec;

  // Since |this| holds a reference to the |cdm_|, by the time the CDM is
  // destructed, UnregisterPlayer() must have been called and |this| has been
  // destructed as well. So the |cdm_unset_cb| will never have a chance to be
  // called.
  // TODO(xhwang): Remove |cdm_unset_cb| after it's not used on all platforms.
  cdm_registration_id_ = media_crypto_context_->RegisterPlayer(
      media::BindToCurrentLoop(base::Bind(&MediaCodecVideoDecoder::OnKeyAdded,
                                          weak_factory_.GetWeakPtr())),
      base::DoNothing());

  // Request a secure surface in all cases.  For L3, it's okay if we fall back
  // to TextureOwner rather than fail composition.  For L1, it's required.
  surface_chooser_helper_.SetSecureSurfaceMode(
      requires_secure_video_codec
          ? SurfaceChooserHelper::SecureSurfaceMode::kRequired
          : SurfaceChooserHelper::SecureSurfaceMode::kRequested);

  // Signal success, and create the codec lazily on the first decode.
  init_cb.Run(true);
}

void MediaCodecVideoDecoder::OnKeyAdded() {
  DVLOG(2) << __func__;
  waiting_for_key_ = false;
  StartTimerOrPumpCodec();
}

void MediaCodecVideoDecoder::StartLazyInit() {
  DVLOG(2) << __func__;
  TRACE_EVENT0("media", "MediaCodecVideoDecoder::StartLazyInit");
  lazy_init_pending_ = false;
  codec_allocator_->StartThread(this);

  // SurfaceControl allows TextureOwner to be promoted to an overlay in the
  // compositing pipeline itself.
  const bool use_texture_owner_as_overlays = is_surface_control_enabled_;

  // Only ask for promotion hints if we can actually switch surfaces, since we
  // wouldn't be able to do anything with them. Also, if threaded texture
  // mailboxes are enabled, then we turn off overlays anyway. And if texture
  // owner can be used as an overlay, no promotion hints are necessary.
  const bool want_promotion_hints =
      device_info_->IsSetOutputSurfaceSupported() &&
      !enable_threaded_texture_mailboxes_ && !use_texture_owner_as_overlays;
  video_frame_factory_->Initialize(
      want_promotion_hints, use_texture_owner_as_overlays,
      base::Bind(&MediaCodecVideoDecoder::OnVideoFrameFactoryInitialized,
                 weak_factory_.GetWeakPtr()));
}

void MediaCodecVideoDecoder::OnVideoFrameFactoryInitialized(
    scoped_refptr<TextureOwner> texture_owner) {
  DVLOG(2) << __func__;
  TRACE_EVENT0("media",
               "MediaCodecVideoDecoder::OnVideoFrameFactoryInitialized");
  if (!texture_owner) {
    EnterTerminalState(State::kError);
    return;
  }
  texture_owner_bundle_ = new AVDASurfaceBundle(std::move(texture_owner));

  // Overlays are disabled when |enable_threaded_texture_mailboxes| is true
  // (http://crbug.com/582170).
  if (enable_threaded_texture_mailboxes_ ||
      !device_info_->SupportsOverlaySurfaces()) {
    OnSurfaceChosen(nullptr);
    return;
  }

  // Request OverlayInfo updates. Initialization continues on the first one.
  bool restart_for_transitions = !device_info_->IsSetOutputSurfaceSupported();
  std::move(request_overlay_info_cb_)
      .Run(restart_for_transitions,
           base::Bind(&MediaCodecVideoDecoder::OnOverlayInfoChanged,
                      weak_factory_.GetWeakPtr()));
}

void MediaCodecVideoDecoder::OnOverlayInfoChanged(
    const OverlayInfo& overlay_info) {
  DVLOG(2) << __func__;
  DCHECK(device_info_->SupportsOverlaySurfaces());
  DCHECK(!enable_threaded_texture_mailboxes_);
  if (InTerminalState())
    return;

  bool overlay_changed = !overlay_info_.RefersToSameOverlayAs(overlay_info);
  overlay_info_ = overlay_info;
  surface_chooser_helper_.SetIsFullscreen(overlay_info_.is_fullscreen);
  surface_chooser_helper_.UpdateChooserState(
      overlay_changed ? base::make_optional(CreateOverlayFactoryCb())
                      : base::nullopt);
}

void MediaCodecVideoDecoder::OnSurfaceChosen(
    std::unique_ptr<AndroidOverlay> overlay) {
  DVLOG(2) << __func__;
  DCHECK(state_ == State::kInitializing ||
         device_info_->IsSetOutputSurfaceSupported());
  TRACE_EVENT1("media", "MediaCodecVideoDecoder::OnSurfaceChosen", "overlay",
               overlay ? "yes" : "no");

  if (overlay) {
    overlay->AddSurfaceDestroyedCallback(
        base::Bind(&MediaCodecVideoDecoder::OnSurfaceDestroyed,
                   weak_factory_.GetWeakPtr()));
    target_surface_bundle_ = new AVDASurfaceBundle(std::move(overlay));
  } else {
    target_surface_bundle_ = texture_owner_bundle_;
  }

  // If we were waiting for our first surface during initialization, then
  // proceed to create a codec.
  if (state_ == State::kInitializing) {
    state_ = State::kRunning;
    CreateCodec();
  }
}

void MediaCodecVideoDecoder::OnSurfaceDestroyed(AndroidOverlay* overlay) {
  DVLOG(2) << __func__;
  DCHECK_NE(state_, State::kInitializing);
  TRACE_EVENT0("media", "MediaCodecVideoDecoder::OnSurfaceDestroyed");

  // If SetOutputSurface() is not supported we only ever observe destruction of
  // a single overlay so this must be the one we're using. In this case it's
  // the responsibility of our consumer to destroy us for surface transitions.
  // TODO(liberato): This might not be true for L1 / L3, since our caller has
  // no idea that this has happened.  We should unback the frames here.  This
  // might work now that we have CodecImageGroup -- verify this.
  if (!device_info_->IsSetOutputSurfaceSupported()) {
    EnterTerminalState(State::kSurfaceDestroyed);
    return;
  }

  // Reset the target bundle if it is the one being destroyed.
  if (target_surface_bundle_ &&
      target_surface_bundle_->overlay.get() == overlay) {
    target_surface_bundle_ = texture_owner_bundle_;
  }

  // Transition the codec away from the overlay if necessary.
  if (SurfaceTransitionPending())
    TransitionToTargetSurface();
}

bool MediaCodecVideoDecoder::SurfaceTransitionPending() {
  return codec_ && codec_->SurfaceBundle() != target_surface_bundle_;
}

void MediaCodecVideoDecoder::TransitionToTargetSurface() {
  DVLOG(2) << __func__;
  DCHECK(SurfaceTransitionPending());
  DCHECK(device_info_->IsSetOutputSurfaceSupported());

  if (!codec_->SetSurface(target_surface_bundle_)) {
    video_frame_factory_->SetSurfaceBundle(nullptr);
    EnterTerminalState(State::kError);
    return;
  }

  video_frame_factory_->SetSurfaceBundle(target_surface_bundle_);
  CacheFrameInformation();
}

void MediaCodecVideoDecoder::CreateCodec() {
  DCHECK(!codec_);
  DCHECK(target_surface_bundle_);
  DCHECK_EQ(state_, State::kRunning);

  scoped_refptr<CodecConfig> config = new CodecConfig();
  config->codec = decoder_config_.codec();
  config->csd0 = csd0_;
  config->csd1 = csd1_;
  config->requires_secure_codec = requires_secure_codec_;
  // TODO(liberato): per android_util.h, remove JavaObjectPtr.
  config->media_crypto =
      std::make_unique<base::android::ScopedJavaGlobalRef<jobject>>(
          media_crypto_);
  config->initial_expected_coded_size = decoder_config_.coded_size();
  config->surface_bundle = target_surface_bundle_;
  config->container_color_space = decoder_config_.color_space_info();
  config->hdr_metadata = decoder_config_.hdr_metadata();

  // Use the asynchronous API if we can.
  if (device_info_->IsAsyncApiSupported()) {
    using_async_api_ = true;
    config->on_buffers_available_cb = BindToCurrentLoop(
        base::BindRepeating(&MediaCodecVideoDecoder::StartTimerOrPumpCodec,
                            weak_factory_.GetWeakPtr()));
  }

  // Note that this might be the same surface bundle that we've been using, if
  // we're reinitializing the codec without changing surfaces.  That's fine.
  video_frame_factory_->SetSurfaceBundle(target_surface_bundle_);
  codec_allocator_->CreateMediaCodecAsync(
      codec_allocator_weak_factory_.GetWeakPtr(), std::move(config));
}

void MediaCodecVideoDecoder::OnCodecConfigured(
    std::unique_ptr<MediaCodecBridge> codec,
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  DCHECK(!codec_);
  DCHECK_EQ(state_, State::kRunning);

  if (!codec) {
    EnterTerminalState(State::kError);
    return;
  }

  codec_ = std::make_unique<CodecWrapper>(
      CodecSurfacePair(std::move(codec), std::move(surface_bundle)),
      base::BindRepeating(&OutputBufferReleased, using_async_api_,
                          BindToCurrentLoop(base::BindRepeating(
                              &MediaCodecVideoDecoder::StartTimerOrPumpCodec,
                              weak_factory_.GetWeakPtr()))),
      base::SequencedTaskRunnerHandle::Get());

  // If the target surface changed while codec creation was in progress,
  // transition to it immediately.
  // Note: this can only happen if we support SetOutputSurface() because if we
  // don't OnSurfaceDestroyed() cancels codec creations, and
  // |surface_chooser_| doesn't change the target surface.
  if (SurfaceTransitionPending())
    TransitionToTargetSurface();

  // Cache the frame information that goes with this codec.
  CacheFrameInformation();

  StartTimerOrPumpCodec();
}

void MediaCodecVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    const DecodeCB& decode_cb) {
  DVLOG(3) << __func__ << ": " << buffer->AsHumanReadableString();
  if (state_ == State::kError) {
    decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }
  pending_decodes_.emplace_back(std::move(buffer), std::move(decode_cb));

  if (state_ == State::kInitializing) {
    if (lazy_init_pending_)
      StartLazyInit();
    return;
  }
  PumpCodec(true);
}

void MediaCodecVideoDecoder::FlushCodec() {
  DVLOG(2) << __func__;

  // If a deferred flush was pending, then it isn't anymore.
  deferred_flush_pending_ = false;

  if (!codec_ || codec_->IsFlushed())
    return;

  if (codec_->SupportsFlush(device_info_)) {
    DVLOG(2) << "Flushing codec";
    if (!codec_->Flush())
      EnterTerminalState(State::kError);
  } else {
    DVLOG(2) << "flush() workaround: creating a new codec";
    // Release the codec and create a new one.
    // Note: we may end up with two codecs attached to the same surface if the
    // release hangs on one thread and create proceeds on another. This will
    // result in an error, letting the user retry the playback. The alternative
    // of waiting for the release risks hanging the playback forever.
    ReleaseCodec();
    CreateCodec();
  }
}

void MediaCodecVideoDecoder::PumpCodec(bool force_start_timer) {
  DVLOG(4) << __func__;
  bool did_work = false, did_input = false, did_output = false;
  do {
    did_input = QueueInput();
    did_output = DequeueOutput();
    if (did_input || did_output)
      did_work = true;
  } while (did_input || did_output);

  if (using_async_api_)
    return;

  if (did_work || force_start_timer)
    StartTimerOrPumpCodec();
  else
    StopTimerIfIdle();
}

void MediaCodecVideoDecoder::StartTimerOrPumpCodec() {
  DVLOG(4) << __func__;
  if (state_ != State::kRunning)
    return;

  if (using_async_api_) {
    PumpCodec(false);
    return;
  }

  idle_timer_ = base::ElapsedTimer();

  // Poll at 10ms somewhat arbitrarily.
  // TODO: Don't poll on new devices; use the callback API.
  // TODO: Experiment with this number to save power. Since we already pump the
  // codec in response to receiving a decode and output buffer release, polling
  // at this frequency is likely overkill in the steady state.
  const auto kPollingPeriod = base::TimeDelta::FromMilliseconds(10);
  if (!pump_codec_timer_.IsRunning()) {
    pump_codec_timer_.Start(FROM_HERE, kPollingPeriod,
                            base::Bind(&MediaCodecVideoDecoder::PumpCodec,
                                       base::Unretained(this), false));
  }
}

void MediaCodecVideoDecoder::StopTimerIfIdle() {
  DVLOG(4) << __func__;
  DCHECK(!using_async_api_);

  // Stop the timer if we've been idle for one second. Chosen arbitrarily.
  const auto kTimeout = base::TimeDelta::FromSeconds(1);
  if (idle_timer_.Elapsed() > kTimeout) {
    DVLOG(2) << "Stopping timer; idle timeout hit";
    pump_codec_timer_.Stop();
    // Draining for destroy can no longer proceed if the timer is stopping,
    // because no more Decode() calls can be made, so complete it now to avoid
    // leaking |this|.
    if (drain_type_ == DrainType::kForDestroy)
      OnCodecDrained();
  }
}

bool MediaCodecVideoDecoder::QueueInput() {
  DVLOG(4) << __func__;
  if (!codec_ || waiting_for_key_)
    return false;

  // If the codec is drained, flush it when there is a pending decode and no
  // unreleased output buffers. This lets us avoid both unbacking frames when we
  // flush, and flushing unnecessarily, like at EOS.
  //
  // Often, we'll elide the eos to drain the codec, but we want to pretend that
  // we did.  In this case, we should also flush.
  if (codec_->IsDrained() || deferred_flush_pending_) {
    if (!codec_->HasUnreleasedOutputBuffers() && !pending_decodes_.empty()) {
      FlushCodec();
      return true;
    }
    return false;
  }

  if (pending_decodes_.empty())
    return false;

  PendingDecode& pending_decode = pending_decodes_.front();
  auto status = codec_->QueueInputBuffer(*pending_decode.buffer,
                                         decoder_config_.encryption_scheme());
  DVLOG((status == CodecWrapper::QueueStatus::kTryAgainLater ||
                 status == CodecWrapper::QueueStatus::kOk
             ? 3
             : 2))
      << "QueueInput(" << pending_decode.buffer->AsHumanReadableString()
      << ") status=" << static_cast<int>(status);

  switch (status) {
    case CodecWrapper::QueueStatus::kOk:
      break;
    case CodecWrapper::QueueStatus::kTryAgainLater:
      return false;
    case CodecWrapper::QueueStatus::kNoKey:
      // Retry when a key is added.
      waiting_for_key_ = true;
      return false;
    case CodecWrapper::QueueStatus::kError:
      EnterTerminalState(State::kError);
      return false;
  }

  if (pending_decode.buffer->end_of_stream()) {
    // The VideoDecoder interface requires that the EOS DecodeCB is called after
    // all decodes before it are delivered, so we have to save it and call it
    // when the EOS is dequeued.
    DCHECK(!eos_decode_cb_);
    eos_decode_cb_ = std::move(pending_decode.decode_cb);
  } else {
    pending_decode.decode_cb.Run(DecodeStatus::OK);
  }
  pending_decodes_.pop_front();
  return true;
}

bool MediaCodecVideoDecoder::DequeueOutput() {
  DVLOG(4) << __func__;
  if (!codec_ || codec_->IsDrained() || waiting_for_key_)
    return false;

  // If a surface transition is pending, wait for all outstanding buffers to be
  // released before doing the transition. This is necessary because the
  // VideoFrames corresponding to these buffers have metadata flags specific to
  // the surface type, and changing the surface before they're rendered would
  // invalidate them.
  if (SurfaceTransitionPending()) {
    if (!codec_->HasUnreleasedOutputBuffers()) {
      TransitionToTargetSurface();
      return true;
    }
    return false;
  }

  base::TimeDelta presentation_time;
  bool eos = false;
  std::unique_ptr<CodecOutputBuffer> output_buffer;
  auto status =
      codec_->DequeueOutputBuffer(&presentation_time, &eos, &output_buffer);
  switch (status) {
    case CodecWrapper::DequeueStatus::kOk:
      break;
    case CodecWrapper::DequeueStatus::kTryAgainLater:
      return false;
    case CodecWrapper::DequeueStatus::kError:
      DVLOG(1) << "DequeueOutputBuffer() error";
      EnterTerminalState(State::kError);
      return false;
  }
  DVLOG(3) << "DequeueOutputBuffer(): pts="
           << (eos ? "EOS"
                   : std::to_string(presentation_time.InMilliseconds()));

  if (eos) {
    if (eos_decode_cb_) {
      // Schedule the EOS DecodeCB to run after all previous frames.
      video_frame_factory_->RunAfterPendingVideoFrames(
          base::Bind(&MediaCodecVideoDecoder::RunEosDecodeCb,
                     weak_factory_.GetWeakPtr(), reset_generation_));
    }
    if (drain_type_)
      OnCodecDrained();
    // We don't flush the drained codec immediately because it might be
    // backing unrendered frames near EOS. It's flushed lazily in QueueInput().
    return false;
  }

  // If we're draining for reset or destroy we can discard |output_buffer|
  // without rendering it.  This is also true if we elided the drain itself,
  // and deferred a flush that would have happened when the drain completed.
  if (drain_type_ || deferred_flush_pending_)
    return true;

  // Record the frame type that we're sending and some information about why.
  UMA_HISTOGRAM_ENUMERATION(
      "Media.AVDA.FrameInformation", cached_frame_information_,
      static_cast<int>(
          SurfaceChooserHelper::FrameInformation::FRAME_INFORMATION_MAX) +
          1);  // PRESUBMIT_IGNORE_UMA_MAX

  gfx::Rect visible_rect(output_buffer->size());
  std::unique_ptr<ScopedAsyncTrace> async_trace =
      ScopedAsyncTrace::CreateIfEnabled(
          "media", "MediaCodecVideoDecoder::CreateVideoFrame");
  video_frame_factory_->CreateVideoFrame(
      std::move(output_buffer), presentation_time,
      GetNaturalSize(visible_rect, decoder_config_.GetPixelAspectRatio()),
      CreatePromotionHintCB(),
      base::BindOnce(&MediaCodecVideoDecoder::ForwardVideoFrame,
                     weak_factory_.GetWeakPtr(), reset_generation_,
                     std::move(async_trace)));
  return true;
}

void MediaCodecVideoDecoder::RunEosDecodeCb(int reset_generation) {
  // Both of the following conditions are necessary because:
  //  * In an error state, the reset generations will match but |eos_decode_cb_|
  //    will be aborted.
  //  * After a Reset(), the reset generations won't match, but we might already
  //    have a new |eos_decode_cb_| for the new generation.
  if (reset_generation == reset_generation_ && eos_decode_cb_)
    std::move(eos_decode_cb_).Run(DecodeStatus::OK);
}

void MediaCodecVideoDecoder::ForwardVideoFrame(
    int reset_generation,
    std::unique_ptr<ScopedAsyncTrace> async_trace,
    const scoped_refptr<VideoFrame>& frame) {
  DVLOG(3) << __func__ << " : "
           << (frame ? frame->AsHumanReadableString() : "null");
  if (reset_generation == reset_generation_) {
    // TODO(liberato): We might actually have a SW decoder.  Consider setting
    // this to false if so, especially for higher bitrates.
    frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, true);
    output_cb_.Run(frame);
  }
}

// Our Reset() provides a slightly stronger guarantee than VideoDecoder does.
// After |closure| runs:
// 1) no VideoFrames from before the Reset() will be output, and
// 2) no DecodeCBs (including EOS) from before the Reset() will be run.
void MediaCodecVideoDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << __func__;
  DCHECK(!reset_cb_);
  reset_generation_++;
  reset_cb_ = std::move(closure);
  CancelPendingDecodes(DecodeStatus::ABORTED);
  StartDrainingCodec(DrainType::kForReset);
}

void MediaCodecVideoDecoder::StartDrainingCodec(DrainType drain_type) {
  DVLOG(2) << __func__;
  TRACE_EVENT0("media", "MediaCodecVideoDecoder::StartDrainingCodec");
  DCHECK(pending_decodes_.empty());
  // It's okay if there's already a drain ongoing. We'll only enqueue an EOS if
  // the codec isn't already draining.
  drain_type_ = drain_type;

  // We can safely invalidate outstanding buffers for both types of drain, and
  // doing so can only make the drain complete quicker.  Note that we do this
  // even if we're eliding the drain, since we're either going to flush the
  // codec or destroy it.  While we're not required to do this, it might affect
  // stability if we don't (https://crbug.com/869365).  AVDA, in particular,
  // dropped all pending codec output buffers when starting a reset (seek) or
  // a destroy.
  if (codec_)
    codec_->DiscardOutputBuffers();

  // Skip the drain if possible. Only VP8 codecs need draining because
  // they can hang in release() or flush() otherwise
  // (http://crbug.com/598963).
  // TODO(watk): Strongly consider blacklisting VP8 (or specific MediaCodecs)
  // instead. Draining is responsible for a lot of complexity.
  if (decoder_config_.codec() != kCodecVP8 || !codec_ || codec_->IsFlushed() ||
      codec_->IsDrained()) {
    // If the codec isn't already drained or flushed, then we have to remember
    // that we owe it a flush.  We also have to remember not to deliver any
    // output buffers that might still be in progress in the codec.
    deferred_flush_pending_ =
        codec_ && !codec_->IsDrained() && !codec_->IsFlushed();
    OnCodecDrained();
    return;
  }

  // Queue EOS if the codec isn't already processing one.
  if (!codec_->IsDraining())
    pending_decodes_.push_back(PendingDecode::CreateEos());

  PumpCodec(true);
}

void MediaCodecVideoDecoder::OnCodecDrained() {
  DVLOG(2) << __func__;
  TRACE_EVENT0("media", "MediaCodecVideoDecoder::OnCodecDrained");
  DrainType drain_type = *drain_type_;
  drain_type_.reset();

  if (drain_type == DrainType::kForDestroy) {
    // Post the delete in case the caller uses |this| after we return.
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    return;
  }

  std::move(reset_cb_).Run();

  // Flush the codec unless (a) it's already flushed, (b) it's drained and the
  // flush will be handled automatically on the next decode, or (c) we've
  // elided the eos and want to defer the flush.
  if (codec_ && !codec_->IsFlushed() && !codec_->IsDrained() &&
      !deferred_flush_pending_) {
    FlushCodec();
  }
}

void MediaCodecVideoDecoder::EnterTerminalState(State state) {
  DVLOG(2) << __func__ << " " << static_cast<int>(state);

  state_ = state;
  DCHECK(InTerminalState());

  // Cancel pending codec creation.
  codec_allocator_weak_factory_.InvalidateWeakPtrs();
  pump_codec_timer_.Stop();
  ReleaseCodec();
  target_surface_bundle_ = nullptr;
  texture_owner_bundle_ = nullptr;
  if (state == State::kError)
    CancelPendingDecodes(DecodeStatus::DECODE_ERROR);
  if (drain_type_)
    OnCodecDrained();
}

bool MediaCodecVideoDecoder::InTerminalState() {
  return state_ == State::kSurfaceDestroyed || state_ == State::kError;
}

void MediaCodecVideoDecoder::CancelPendingDecodes(DecodeStatus status) {
  for (auto& pending_decode : pending_decodes_)
    pending_decode.decode_cb.Run(status);
  pending_decodes_.clear();
  if (eos_decode_cb_)
    std::move(eos_decode_cb_).Run(status);
}

void MediaCodecVideoDecoder::ReleaseCodec() {
  if (!codec_)
    return;
  auto pair = codec_->TakeCodecSurfacePair();
  codec_ = nullptr;
  codec_allocator_->ReleaseMediaCodec(std::move(pair.first),
                                      std::move(pair.second));
}

AndroidOverlayFactoryCB MediaCodecVideoDecoder::CreateOverlayFactoryCb() {
  if (!overlay_factory_cb_ || !overlay_info_.HasValidRoutingToken())
    return AndroidOverlayFactoryCB();

  return base::BindRepeating(overlay_factory_cb_, *overlay_info_.routing_token);
}

std::string MediaCodecVideoDecoder::GetDisplayName() const {
  return "MediaCodecVideoDecoder";
}

bool MediaCodecVideoDecoder::NeedsBitstreamConversion() const {
  return true;
}

bool MediaCodecVideoDecoder::CanReadWithoutStalling() const {
  // MediaCodec gives us no indication that it will stop producing outputs
  // until we provide more inputs or release output buffers back to it, so
  // we have to always return false.
  // TODO(watk): This puts all MCVD playbacks into low delay mode (i.e., the
  // renderer won't try to preroll). Ideally we'd be smarter about
  // this and attempt preroll but be able to give up if we can't produce
  // enough frames.
  return false;
}

int MediaCodecVideoDecoder::GetMaxDecodeRequests() const {
  // We indicate that we're done decoding a frame as soon as we submit it to
  // MediaCodec so the number of parallel decode requests just sets the upper
  // limit of the size of our pending decode queue.
  return 2;
}

PromotionHintAggregator::NotifyPromotionHintCB
MediaCodecVideoDecoder::CreatePromotionHintCB() {
  // Right now, we don't request promotion hints.  This is only used by SOP.
  // While we could simplify it a bit, this is the general form that we'll use
  // when handling promotion hints.

  // Note that this keeps only a wp to the surface bundle via |layout_cb|.  It
  // also continues to work even if |this| is destroyed; images might want to
  // move an overlay around even after MCVD has been torn down.  For example
  // inline L1 content will fall into this case.
  return BindToCurrentLoop(base::BindRepeating(
      [](base::WeakPtr<MediaCodecVideoDecoder> mcvd,
         AVDASurfaceBundle::ScheduleLayoutCB layout_cb,
         PromotionHintAggregator::Hint hint) {
        // If we're promotable, and we have a surface bundle, then also
        // position the overlay.  We could do this even if the overlay is
        // not promotable, but it wouldn't have any visible effect.
        if (hint.is_promotable)
          layout_cb.Run(hint.screen_rect);

        // Notify MCVD about the promotion hint, so that it can decide if it
        // wants to switch to / from an overlay.
        if (mcvd)
          mcvd->NotifyPromotionHint(hint);
      },
      weak_factory_.GetWeakPtr(),
      codec_->SurfaceBundle()->GetScheduleLayoutCB()));
}

bool MediaCodecVideoDecoder::IsUsingOverlay() const {
  return codec_ && codec_->SurfaceBundle() && codec_->SurfaceBundle()->overlay;
}

void MediaCodecVideoDecoder::NotifyPromotionHint(
    PromotionHintAggregator::Hint hint) {
  surface_chooser_helper_.NotifyPromotionHintAndUpdateChooser(hint,
                                                              IsUsingOverlay());
}

void MediaCodecVideoDecoder::CacheFrameInformation() {
  cached_frame_information_ =
      surface_chooser_helper_.ComputeFrameInformation(IsUsingOverlay());
}

}  // namespace media
