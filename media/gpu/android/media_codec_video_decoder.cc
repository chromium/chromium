// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/media_codec_video_decoder.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/async_destroy_video_decoder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/scoped_async_trace.h"
#include "media/base/status.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/android_video_surface_chooser.h"
#include "media/gpu/android/codec_allocator.h"
#include "media/gpu/android/video_accelerator_util.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/base/android/extract_sps_and_pps.h"
#endif

namespace media {
namespace {

void OutputBufferReleased(bool using_async_api,
                          base::RepeatingClosure pump_cb,
                          bool has_work) {
  // The asynchronous API doesn't need pumping upon calls to ReleaseOutputBuffer
  // unless we're draining or drained.
  if (using_async_api && !has_work)
    return;
  pump_cb.Run();
}

bool IsSurfaceControlEnabled(const gpu::GpuFeatureInfo& info) {
  return info.status_values[gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] ==
         gpu::kGpuFeatureStatusEnabled;
}

std::vector<SupportedVideoDecoderConfig> GetSupportedConfigsInternal(
    DeviceInfo* device_info,
    bool allow_software_codecs = false) {
  std::vector<SupportedVideoDecoderConfig> supported_configs;
  for (const auto& info : GetDecoderInfoCache()) {
    const auto codec = VideoCodecProfileToVideoCodec(info.profile);
    if ((codec == VideoCodec::kVP8 && device_info->IsVp8DecoderAvailable()) ||
        (codec == VideoCodec::kVP9 && device_info->IsVp9DecoderAvailable()) ||
        (codec == VideoCodec::kAV1 && device_info->IsAv1DecoderAvailable())) {
      // We don't compile support into libvpx for these profiles, so allow them
      // for all resolutions. Tests require availability of software codecs.
      const bool require_encrypted = !allow_software_codecs &&
                                     info.profile != VP9PROFILE_PROFILE2 &&
                                     info.profile != VP9PROFILE_PROFILE3;
      supported_configs.emplace_back(
          info.profile, info.profile, info.coded_size_min, info.coded_size_max,
          /*allow_encrypted=*/true, require_encrypted);

      // Don't allow software decoding since we bundle VP8, VP9, AV1 decoders.
      if (!info.is_software_codec && require_encrypted) {
        // Require a minimum of 360p even for hardware decoding of VP8, VP9.
        auto coded_size_min = info.coded_size_min;
        if (codec == VideoCodec::kVP8 || codec == VideoCodec::kVP9) {
          coded_size_min.SetToMax(gfx::Size(360, 360));
        }
        supported_configs.emplace_back(info.profile, info.profile,
                                       coded_size_min, info.coded_size_max,
                                       /*allow_encrypted=*/false,
                                       /*require_encrypted=*/false);
      }
      continue;
    }
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    if (codec == VideoCodec::kH264
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
        || (codec == VideoCodec::kHEVC &&
            base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport))
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
        || codec == VideoCodec::kDolbyVision
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    ) {
      supported_configs.emplace_back(info.profile, info.profile,
                                     info.coded_size_min, info.coded_size_max,
                                     /*allow_encrypted=*/true,
                                     /*require_encrypted=*/false);
    }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  }

  return supported_configs;
}

// Return the name of the decoder that will be used to create MediaCodec.
std::string SelectMediaCodec(const VideoDecoderConfig& config,
                             bool requires_secure_codec) {
  std::string software_decoder;
  for (const auto& info : GetDecoderInfoCache()) {
    VideoCodec codec = VideoCodecProfileToVideoCodec(info.profile);
    if (config.codec() != codec) {
      continue;
    }

    if (config.profile() != VIDEO_CODEC_PROFILE_UNKNOWN &&
        config.profile() != info.profile) {
      continue;
    }

    if (config.level() != kNoVideoCodecLevel && config.level() > info.level) {
      continue;
    }

    if (config.coded_size().width() < info.coded_size_min.width() ||
        config.coded_size().height() < info.coded_size_min.height() ||
        config.coded_size().width() > info.coded_size_max.width() ||
        config.coded_size().height() > info.coded_size_max.width()) {
      continue;
    }

    if (!requires_secure_codec &&
        info.secure_codec_capability == SecureCodecCapability::kEncrypted) {
      continue;
    }

    if (requires_secure_codec &&
        info.secure_codec_capability == SecureCodecCapability::kClear) {
      continue;
    }

    // Prioritize hardware decoder. Software decoder will be selected as a
    // fallback option.
    if (info.is_software_codec) {
      if (software_decoder.empty()) {
        software_decoder = info.name;
      }
      continue;
    }

    return info.name;
  }

  // Allow software decoder if either:
  // 1. the stream is encrypted.
  // 2. No software decoder is bundled into Chromium.
  if (!(config.is_encrypted()
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        || config.codec() == VideoCodec::kH264
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
        || (config.codec() == VideoCodec::kHEVC &&
            base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport))
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
        || config.codec() == VideoCodec::kDolbyVision
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
        )) {
    software_decoder = "";
  }

  DVLOG_IF(2, software_decoder.empty())
      << "Can't find proper video decoder from decoder info cache, "
         "fallback to the default decoder selection path.";
  return software_decoder;
}

}  // namespace

// When re-initializing the codec changes the resolution to be more than
// |kReallocateThreshold| times the old one, force a codec reallocation to
// update the hints that we provide to MediaCodec.  crbug.com/989182 .
constexpr static float kReallocateThreshold = 3.9;

// static
PendingDecode PendingDecode::CreateEos() {
  return {DecoderBuffer::CreateEOSBuffer(), base::DoNothing()};
}

PendingDecode::PendingDecode(scoped_refptr<DecoderBuffer> buffer,
                             VideoDecoder::DecodeCB decode_cb)
    : buffer(std::move(buffer)), decode_cb(std::move(decode_cb)) {}
PendingDecode::PendingDecode(PendingDecode&& other) = default;
PendingDecode::~PendingDecode() = default;

// static
std::vector<SupportedVideoDecoderConfig>
MediaCodecVideoDecoder::GetSupportedConfigs() {
  static const auto configs =
      GetSupportedConfigsInternal(DeviceInfo::GetInstance());
  return configs;
}

MediaCodecVideoDecoder::MediaCodecVideoDecoder(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    std::unique_ptr<MediaLog> media_log,
    DeviceInfo* device_info,
    CodecAllocator* codec_allocator,
    std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
    AndroidOverlayMojoFactoryCB overlay_factory_cb,
    RequestOverlayInfoCB request_overlay_info_cb,
    std::unique_ptr<VideoFrameFactory> video_frame_factory,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
      media_log_(std::move(media_log)),
      codec_allocator_(codec_allocator),
      request_overlay_info_cb_(std::move(request_overlay_info_cb)),
      is_surface_control_enabled_(IsSurfaceControlEnabled(gpu_feature_info)),
      surface_chooser_helper_(
          std::move(surface_chooser),
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kForceVideoOverlays),
          base::FeatureList::IsEnabled(media::kUseAndroidOverlayForSecureOnly),
          is_surface_control_enabled_),
      video_frame_factory_(std::move(video_frame_factory)),
      overlay_factory_cb_(std::move(overlay_factory_cb)),
      device_info_(device_info),
      enable_threaded_texture_mailboxes_(
          gpu_preferences.enable_threaded_texture_mailboxes),
      allow_nonsecure_overlays_(
          base::FeatureList::IsEnabled(media::kAllowNonSecureOverlays)) {
  DVLOG(2) << __func__;
  surface_chooser_helper_.chooser()->SetClientCallbacks(
      base::BindRepeating(&MediaCodecVideoDecoder::OnSurfaceChosen,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&MediaCodecVideoDecoder::OnSurfaceChosen,
                          weak_factory_.GetWeakPtr(), nullptr));
}

std::unique_ptr<VideoDecoder> MediaCodecVideoDecoder::Create(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    std::unique_ptr<MediaLog> media_log,
    DeviceInfo* device_info,
    CodecAllocator* codec_allocator,
    std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
    AndroidOverlayMojoFactoryCB overlay_factory_cb,
    RequestOverlayInfoCB request_overlay_info_cb,
    std::unique_ptr<VideoFrameFactory> video_frame_factory,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  auto* decoder = new MediaCodecVideoDecoder(
      gpu_preferences, gpu_feature_info, std::move(media_log), device_info,
      codec_allocator, std::move(surface_chooser),
      std::move(overlay_factory_cb), std::move(request_overlay_info_cb),
      std::move(video_frame_factory), std::move(drdc_lock));
  return std::make_unique<AsyncDestroyVideoDecoder<MediaCodecVideoDecoder>>(
      base::WrapUnique(decoder));
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  DVLOG(2) << __func__;
  TRACE_EVENT0("media", "MediaCodecVideoDecoder::~MediaCodecVideoDecoder");
  ReleaseCodec();
}

void MediaCodecVideoDecoder::DestroyAsync(
    std::unique_ptr<MediaCodecVideoDecoder> decoder) {
  DVLOG(1) << __func__;
  TRACE_EVENT0("media", "MediaCodecVideoDecoder::Destroy");
  DCHECK(decoder);

  // This will be destroyed by a call to |DeleteSoon|
  // in |OnCodecDrained|.
  auto* self = decoder.release();

  // Cancel pending callbacks.
  //
  // WARNING: This will lose the callback we've given to MediaCodecBridge for
  // asynchronous notifications; so we must not leave this function with any
  // work necessary from StartTimerOrPumpCodec().
  self->weak_factory_.InvalidateWeakPtrs();

  if (self->media_crypto_context_) {
    // Cancel previously registered callback (if any).
    self->event_cb_registration_.reset();
    self->media_crypto_context_->SetMediaCryptoReadyCB(base::NullCallback());
    self->media_crypto_context_ = nullptr;
  }

  // Mojo callbacks require that they're run before destruction.
  if (self->reset_cb_)
    std::move(self->reset_cb_).Run();

  // Cancel callbacks we no longer want.
  self->codec_allocator_weak_factory_.InvalidateWeakPtrs();
  self->CancelPendingDecodes(DecoderStatus::Codes::kAborted);
  self->StartDrainingCodec(DrainType::kForDestroy);

  // Per the WARNING above. Validate that no draining work remains.
  if (self->using_async_api_)
    DCHECK(!self->drain_type_.has_value());
}

void MediaCodecVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                        bool low_delay,
                                        CdmContext* cdm_context,
                                        InitCB init_cb,
                                        const OutputCB& output_cb,
                                        const WaitingCB& waiting_cb) {
  DCHECK(output_cb);
  DCHECK(waiting_cb);

  const bool first_init = !decoder_config_.IsValidConfig();
  DVLOG(1) << (first_init ? "Initializing" : "Reinitializing")
           << " MCVD with config: " << config.AsHumanReadableString()
           << ", cdm_context = " << cdm_context;

  if (!config.IsValidConfig()) {
    MEDIA_LOG(INFO, media_log_) << "Video configuration is not valid: "
                                << config.AsHumanReadableString();
    DVLOG(1) << "Invalid configuration.";
    base::BindPostTaskToCurrentDefault(std::move(init_cb))
        .Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  // Tests override the DeviceInfo, so if an override is provided query the
  // configs as they look under that DeviceInfo. If not, use the default method
  // which is statically cached for faster Initialize().
  //
  // The tests also require the presence of software codecs.
  const auto configs = device_info_ == DeviceInfo::GetInstance()
                           ? GetSupportedConfigs()
                           : GetSupportedConfigsInternal(
                                 device_info_, /*allow_software_codecs=*/true);
  if (!IsVideoDecoderConfigSupported(configs, config)) {
    DVLOG(1) << "Unsupported configuration.";
    MEDIA_LOG(INFO, media_log_) << "Video configuration is not valid: "
                                << config.AsHumanReadableString();
    base::BindPostTaskToCurrentDefault(std::move(init_cb))
        .Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  // Disallow codec changes when reinitializing.
  if (!first_init && decoder_config_.codec() != config.codec()) {
    DVLOG(1) << "Codec changed: cannot reinitialize";
    MEDIA_LOG(INFO, media_log_) << "Cannot change codec during re-init: "
                                << decoder_config_.AsHumanReadableString()
                                << " -> " << config.AsHumanReadableString();
    base::BindPostTaskToCurrentDefault(std::move(init_cb))
        .Run(DecoderStatus::Codes::kCantChangeCodec);
    return;
  }
  decoder_config_ = config;

  surface_chooser_helper_.SetVideoRotation(
      decoder_config_.video_transformation().rotation);

  output_cb_ = output_cb;
  waiting_cb_ = waiting_cb;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (config.codec() == VideoCodec::kH264)
    ExtractSpsAndPps(config.extra_data(), &csd0_, &csd1_);
#endif

  // We only support setting CDM at first initialization. Even if the initial
  // config is clear, we'll still try to set CDM since we may switch to an
  // encrypted config later.
  const int width = decoder_config_.coded_size().width();
  if (first_init && cdm_context && cdm_context->GetMediaCryptoContext()) {
    DCHECK(media_crypto_.is_null());
    last_width_ = width;
    SetCdm(cdm_context, std::move(init_cb));
    return;
  }

  if (config.is_encrypted() && media_crypto_.is_null()) {
    DVLOG(1) << "No MediaCrypto to handle encrypted config";
    MEDIA_LOG(INFO, media_log_) << "No MediaCrypto to handle encrypted config";
    base::BindPostTaskToCurrentDefault(std::move(init_cb))
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // Do the rest of the initialization lazily on the first decode.
  base::BindPostTaskToCurrentDefault(std::move(init_cb))
      .Run(DecoderStatus::Codes::kOk);

  // On re-init, reallocate the codec if the size has changed too much.
  // Restrict this behavior to Q, where the behavior changed.
  if (first_init) {
    last_width_ = width;
  } else if (width > last_width_ * kReallocateThreshold && device_info_ &&
             device_info_->SdkVersion() > base::android::SDK_VERSION_P) {
    // Reallocate the codec the next time we queue input, once there are no
    // outstanding output buffers.  Note that |deferred_flush_pending_| might
    // already be set, which is fine.  We're just upgrading the flush.
    //
    // If the codec IsDrained(), then we'll flush anyway.  However, just to be
    // sure, request a deferred flush.
    deferred_flush_pending_ = true;
    deferred_reallocation_pending_ = true;
    // Since this will re-use the same surface, allow a retry to work around a
    // race condition in the android framework.
    should_retry_codec_allocation_ = true;
    last_width_ = width;
  }  // else leave |last_width_| unmodified, since we're re-using the codec.
}

void MediaCodecVideoDecoder::SetCdm(CdmContext* cdm_context, InitCB init_cb) {
  DVLOG(1) << __func__;
  DCHECK(cdm_context) << "No CDM provided";
  DCHECK(cdm_context->GetMediaCryptoContext());

  media_crypto_context_ = cdm_context->GetMediaCryptoContext();

  // CdmContext will always post the registered callback back to this thread.
  event_cb_registration_ = cdm_context->RegisterEventCB(base::BindRepeating(
      &MediaCodecVideoDecoder::OnCdmContextEvent, weak_factory_.GetWeakPtr()));

  // The callback will be posted back to this thread via
  // base::BindPostTaskToCurrentDefault.
  media_crypto_context_->SetMediaCryptoReadyCB(
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&MediaCodecVideoDecoder::OnMediaCryptoReady,
                         weak_factory_.GetWeakPtr(), std::move(init_cb))));
}

void MediaCodecVideoDecoder::OnMediaCryptoReady(
    InitCB init_cb,
    JavaObjectPtr media_crypto,
    bool requires_secure_video_codec) {
  DVLOG(1) << __func__
           << ": requires_secure_video_codec = " << requires_secure_video_codec;

  DCHECK(state_ == State::kInitializing);
  DCHECK(media_crypto);

  if (media_crypto->is_null()) {
    media_crypto_context_->SetMediaCryptoReadyCB(base::NullCallback());
    media_crypto_context_ = nullptr;

    if (decoder_config_.is_encrypted()) {
      LOG(ERROR) << "MediaCrypto is not available";
      EnterTerminalState(State::kError, "MediaCrypto is not available");
      std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
      return;
    }

    // MediaCrypto is not available, but the stream is clear. So we can still
    // play the current stream. But if we switch to an encrypted stream playback
    // will fail.
    std::move(init_cb).Run(DecoderStatus::Codes::kOk);
    return;
  }

  media_crypto_ = *media_crypto;
  requires_secure_codec_ = requires_secure_video_codec;

  // Request a secure surface in all cases.  For L3, it's okay if we fall back
  // to TextureOwner rather than fail composition.  For L1, it's required.
  surface_chooser_helper_.SetSecureSurfaceMode(
      requires_secure_video_codec
          ? SurfaceChooserHelper::SecureSurfaceMode::kRequired
          : SurfaceChooserHelper::SecureSurfaceMode::kRequested);

  // Signal success, and create the codec lazily on the first decode.
  std::move(init_cb).Run(DecoderStatus::Codes::kOk);
}

void MediaCodecVideoDecoder::OnCdmContextEvent(CdmContext::Event event) {
  DVLOG(2) << __func__;

  if (event != CdmContext::Event::kHasAdditionalUsableKey)
    return;

  waiting_for_key_ = false;
  StartTimerOrPumpCodec();
}

void MediaCodecVideoDecoder::StartLazyInit() {
  DVLOG(2) << __func__;
  TRACE_EVENT0("media", "MediaCodecVideoDecoder::StartLazyInit");
  lazy_init_pending_ = false;

  // Only ask for promotion hints if we can actually switch surfaces, since we
  // wouldn't be able to do anything with them. Also, if threaded texture
  // mailboxes are enabled, then we turn off overlays anyway.
  const bool want_promotion_hints =
      device_info_->IsSetOutputSurfaceSupported() &&
      !enable_threaded_texture_mailboxes_;

  VideoFrameFactory::OverlayMode overlay_mode =
      VideoFrameFactory::OverlayMode::kDontRequestPromotionHints;
  if (is_surface_control_enabled_) {
    overlay_mode =
        requires_secure_codec_
            ? VideoFrameFactory::OverlayMode::kSurfaceControlSecure
            : VideoFrameFactory::OverlayMode::kSurfaceControlInsecure;
  } else if (want_promotion_hints) {
    overlay_mode = VideoFrameFactory::OverlayMode::kRequestPromotionHints;
  }

  // Regardless of whether we're using SurfaceControl or Dialog overlays, don't
  // allow any overlays in A/B power testing mode, unless this requires a
  // secure surface.  Don't fail the playback for power testing.
  if (!requires_secure_codec_ && !allow_nonsecure_overlays_)
    overlay_mode = VideoFrameFactory::OverlayMode::kDontRequestPromotionHints;

  video_frame_factory_->Initialize(
      overlay_mode, base::BindRepeating(
                        &MediaCodecVideoDecoder::OnVideoFrameFactoryInitialized,
                        weak_factory_.GetWeakPtr()));
}

void MediaCodecVideoDecoder::OnVideoFrameFactoryInitialized(
    scoped_refptr<gpu::TextureOwner> texture_owner) {
  DVLOG(2) << __func__;
  TRACE_EVENT0("media",
               "MediaCodecVideoDecoder::OnVideoFrameFactoryInitialized");
  if (!texture_owner) {
    EnterTerminalState(State::kError, "Could not allocated TextureOwner");
    return;
  }
  texture_owner_bundle_ =
      new CodecSurfaceBundle(std::move(texture_owner), GetDrDcLock());

  // This is for A/B power testing only.  Turn off Dialog-based overlays in
  // power testing mode, unless we need them for L1 content.
  // See https://crbug.com/1081346 .
  const bool allowed_for_experiment =
      requires_secure_codec_ || allow_nonsecure_overlays_;

  // Overlays are disabled when |enable_threaded_texture_mailboxes| is true
  // (http://crbug.com/582170).
  if (enable_threaded_texture_mailboxes_ ||
      !device_info_->SupportsOverlaySurfaces() || !allowed_for_experiment) {
    OnSurfaceChosen(nullptr);
    return;
  }

  // Request OverlayInfo updates. Initialization continues on the first one.
  bool restart_for_transitions = !device_info_->IsSetOutputSurfaceSupported();
  std::move(request_overlay_info_cb_)
      .Run(restart_for_transitions,
           base::BindRepeating(&MediaCodecVideoDecoder::OnOverlayInfoChanged,
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
  surface_chooser_helper_.SetIsPersistentVideo(
      overlay_info_.is_persistent_video);
  surface_chooser_helper_.UpdateChooserState(
      overlay_changed ? absl::make_optional(CreateOverlayFactoryCb())
                      : absl::nullopt);
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
        base::BindOnce(&MediaCodecVideoDecoder::OnSurfaceDestroyed,
                       weak_factory_.GetWeakPtr()));
    target_surface_bundle_ = new CodecSurfaceBundle(std::move(overlay));
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
    EnterTerminalState(State::kSurfaceDestroyed, "Surface destroyed");
    return;
  }

  // Reset the target bundle if it is the one being destroyed.
  if (target_surface_bundle_ && target_surface_bundle_->overlay() == overlay)
    target_surface_bundle_ = texture_owner_bundle_;

  if (requires_secure_codec_ &&
      target_surface_bundle_ == texture_owner_bundle_) {
    // Assume that the target bundle is a SurfaceTexture, or insecure image
    // reader, and reset the codec.  We can't decode anything.  We might want
    // to verify that this isn't a secure image reader, but there's no
    // combination that would create one that would also get here.  Secure
    // image readers are used with SurfaceControl only.
    DVLOG(2) << "surface destroyed for secure codec, resetting MediaCodec.";
    video_frame_factory_->SetSurfaceBundle(nullptr);
    ReleaseCodec();
    waiting_cb_.Run(WaitingReason::kSecureSurfaceLost);
    return;
  }

  // Transition the codec away from the overlay if necessary.  This must be
  // complete before this function returns.
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
    EnterTerminalState(State::kError, "Could not switch codec output surface");
    return;
  }

  video_frame_factory_->SetSurfaceBundle(target_surface_bundle_);
  CacheFrameInformation();
}

void MediaCodecVideoDecoder::CreateCodec() {
  DCHECK(!codec_);
  DCHECK(target_surface_bundle_);
  DCHECK_EQ(state_, State::kRunning);

  auto config = std::make_unique<VideoCodecConfig>();
  if (requires_secure_codec_)
    config->codec_type = CodecType::kSecure;
  config->codec = decoder_config_.codec();
  config->csd0 = csd0_;
  config->csd1 = csd1_;
  config->surface = target_surface_bundle_->GetJavaSurface();
  config->media_crypto = media_crypto_;
  config->initial_expected_coded_size = decoder_config_.coded_size();
  config->container_color_space = decoder_config_.color_space_info();
  config->hdr_metadata = decoder_config_.hdr_metadata();
  config->name = SelectMediaCodec(decoder_config_, requires_secure_codec_);

  // Use the asynchronous API if we can.
  if (device_info_->IsAsyncApiSupported()) {
    using_async_api_ = true;
    config->on_buffers_available_cb = base::BindPostTaskToCurrentDefault(
        base::BindRepeating(&MediaCodecVideoDecoder::StartTimerOrPumpCodec,
                            weak_factory_.GetWeakPtr()));
  }

  // Note that this might be the same surface bundle that we've been using, if
  // we're reinitializing the codec without changing surfaces.  That's fine.
  video_frame_factory_->SetSurfaceBundle(target_surface_bundle_);
  codec_allocator_->CreateMediaCodecAsync(
      base::BindOnce(&MediaCodecVideoDecoder::OnCodecConfiguredInternal,
                     codec_allocator_weak_factory_.GetWeakPtr(),
                     codec_allocator_, target_surface_bundle_),
      std::move(config));
}

// static
void MediaCodecVideoDecoder::OnCodecConfiguredInternal(
    base::WeakPtr<MediaCodecVideoDecoder> weak_this,
    CodecAllocator* codec_allocator,
    scoped_refptr<CodecSurfaceBundle> surface_bundle,
    std::unique_ptr<MediaCodecBridge> codec) {
  if (!weak_this) {
    if (codec) {
      codec_allocator->ReleaseMediaCodec(
          std::move(codec),
          base::BindOnce(
              &base::SequencedTaskRunner::ReleaseSoon<CodecSurfaceBundle>,
              base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
              std::move(surface_bundle)));
    }
    return;
  }
  weak_this->OnCodecConfigured(std::move(surface_bundle), std::move(codec));
}

void MediaCodecVideoDecoder::OnCodecConfigured(
    scoped_refptr<CodecSurfaceBundle> surface_bundle,
    std::unique_ptr<MediaCodecBridge> codec) {
  DCHECK(!codec_);
  DCHECK_EQ(state_, State::kRunning);
  bool should_retry_codec_allocation = should_retry_codec_allocation_;
  should_retry_codec_allocation_ = false;

  // In rare cases, the framework can fail transiently when trying to re-use a
  // surface.  If we're in one of those cases, then retry codec allocation.
  // This only happens on R and S, so skip it otherwise.
  if (!codec && should_retry_codec_allocation &&
      device_info_->SdkVersion() >= base::android::SDK_VERSION_R &&
      device_info_->SdkVersion() <= 32 /* SDK_VERSION_S_V2 */
  ) {
    // We might want to post this with a short delay, but there is already quite
    // a lot of overhead in codec allocation.
    CreateCodec();
    return;
  }

  if (!codec) {
    EnterTerminalState(State::kError, "Unable to allocate codec");
    return;
  }

  max_input_size_ = codec->GetMaxInputSize();
  codec_ = std::make_unique<CodecWrapper>(
      CodecSurfacePair(std::move(codec), std::move(surface_bundle)),
      base::BindRepeating(
          &OutputBufferReleased, using_async_api_,
          base::BindPostTaskToCurrentDefault(base::BindRepeating(
              &MediaCodecVideoDecoder::StartTimerOrPumpCodec,
              weak_factory_.GetWeakPtr()))),
      base::SequencedTaskRunner::GetCurrentDefault(),
      decoder_config_.coded_size());

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
                                    DecodeCB decode_cb) {
  DVLOG(3) << __func__ << ": " << buffer->AsHumanReadableString();
  if (state_ == State::kError) {
    std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (!DecoderBuffer::DoSubsamplesMatch(*buffer)) {
    std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
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

  // Release and re-allocate the codec, if needed, for a resolution change.
  // This also counts as a flush.  Note that we could also stop / configure /
  // start the codec, but there's a fair bit of complexity in that.  Timing
  // tests didn't show any big advantage.  During a resolution change, the time
  // between the next time we queue an input buffer and the next time we get an
  // output buffer were:
  //
  //  flush only:               0.04 s
  //  stop / configure / start: 0.026 s
  //  release / create:         0.03 s
  //
  // So, it seems that flushing the codec defers some work (buffer reallocation
  // or similar) that ends up on the critical path.  I didn't verify what
  // happens when we're flushing without a resolution change, nor can I quite
  // explain how anything can be done off the critical path when a flush is
  // deferred to the first queued input.
  if (deferred_reallocation_pending_) {
    deferred_reallocation_pending_ = false;
    ReleaseCodec();
    // Re-initializing the codec with the same surface may need to retry.
    should_retry_codec_allocation_ = !SurfaceTransitionPending();
    CreateCodec();
  }

  if (!codec_ || codec_->IsFlushed())
    return;

  DVLOG(2) << "Flushing codec";
  if (!codec_->Flush())
    EnterTerminalState(State::kError, "Codec flush failed");
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
  const auto kPollingPeriod = base::Milliseconds(10);
  if (!pump_codec_timer_.IsRunning()) {
    pump_codec_timer_.Start(
        FROM_HERE, kPollingPeriod,
        base::BindRepeating(&MediaCodecVideoDecoder::PumpCodec,
                            base::Unretained(this), false));
  }
}

void MediaCodecVideoDecoder::StopTimerIfIdle() {
  DVLOG(4) << __func__;
  DCHECK(!using_async_api_);

  // Stop the timer if we've been idle for one second. Chosen arbitrarily.
  const auto kTimeout = base::Seconds(1);
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
  if (!pending_decode.buffer->end_of_stream() &&
      pending_decode.buffer->is_key_frame() &&
      pending_decode.buffer->data_size() > max_input_size_) {
    // If we we're already using the provided resolution, try to guess something
    // larger based on the actual input size.
    if (decoder_config_.coded_size().width() == last_width_) {
      // See MediaFormatBuilder::addInputSizeInfoToFormat() for details.
      const size_t compression_ratio =
          (decoder_config_.codec() == VideoCodec::kH264 ||
           decoder_config_.codec() == VideoCodec::kVP8)
              ? 2
              : 4;
      const size_t max_pixels =
          (pending_decode.buffer->data_size() * compression_ratio * 2) / 3;
      if (max_pixels > 8294400)  // 4K
        decoder_config_.set_coded_size(gfx::Size(7680, 4320));
      else if (max_pixels > 2088960)  // 1080p
        decoder_config_.set_coded_size(gfx::Size(3840, 2160));
      else
        decoder_config_.set_coded_size(gfx::Size(1920, 1080));
    }

    // Flush and reallocate on the next call to QueueInput() if we changed size;
    // otherwise just try queuing the buffer and hoping for the best.
    if (decoder_config_.coded_size().width() != last_width_) {
      deferred_flush_pending_ = true;
      deferred_reallocation_pending_ = true;
      last_width_ = decoder_config_.coded_size().width();
      return true;
    }
  }

  auto status = codec_->QueueInputBuffer(*pending_decode.buffer);
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
      waiting_cb_.Run(WaitingReason::kNoDecryptionKey);
      return false;
    case CodecWrapper::QueueStatus::kError:
      EnterTerminalState(State::kError, "QueueInputBuffer failed");
      return false;
  }

  if (pending_decode.buffer->end_of_stream()) {
    // The VideoDecoder interface requires that the EOS DecodeCB is called after
    // all decodes before it are delivered, so we have to save it and call it
    // when the EOS is dequeued.
    DCHECK(!eos_decode_cb_);
    eos_decode_cb_ = std::move(pending_decode.decode_cb);
  } else {
    std::move(pending_decode.decode_cb).Run(DecoderStatus::Codes::kOk);
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
      EnterTerminalState(State::kError, "DequeueOutputBuffer failed");
      return false;
  }
  DVLOG(3) << "DequeueOutputBuffer(): pts="
           << (eos ? "EOS"
                   : std::to_string(presentation_time.InMilliseconds()));

  if (eos) {
    if (eos_decode_cb_) {
      // Schedule the EOS DecodeCB to run after all previous frames.
      video_frame_factory_->RunAfterPendingVideoFrames(
          base::BindOnce(&MediaCodecVideoDecoder::RunEosDecodeCb,
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

  // If we're getting outputs larger than our configured size, we run the risk
  // of exceeding MediaCodec's allowed input buffer size. Update the coded size
  // as we go to ensure we can correctly reconfigure if needed later.
  if (output_buffer->size().GetArea() > decoder_config_.coded_size().GetArea())
    decoder_config_.set_coded_size(output_buffer->size());

  gfx::Rect visible_rect(output_buffer->size());
  std::unique_ptr<ScopedAsyncTrace> async_trace =
      ScopedAsyncTrace::CreateIfEnabled(
          "MediaCodecVideoDecoder::CreateVideoFrame");
  // Make sure that we're notified when this is rendered.  Otherwise, if we're
  // waiting for all output buffers to drain so that we can swap the output
  // surface, we might not realize that we may continue.  If we're using
  // SurfaceControl overlays, then this isn't needed; there is never a surface
  // transition anyway.
  if (!is_surface_control_enabled_) {
    output_buffer->set_render_cb(base::BindPostTaskToCurrentDefault(
        base::BindOnce(&MediaCodecVideoDecoder::StartTimerOrPumpCodec,
                       weak_factory_.GetWeakPtr())));
  }
  video_frame_factory_->CreateVideoFrame(
      std::move(output_buffer), presentation_time,
      decoder_config_.aspect_ratio().GetNaturalSize(visible_rect),
      CreatePromotionHintCB(),
      base::BindOnce(&MediaCodecVideoDecoder::ForwardVideoFrame,
                     weak_factory_.GetWeakPtr(), reset_generation_,
                     std::move(async_trace), base::TimeTicks::Now()));
  return true;
}

void MediaCodecVideoDecoder::RunEosDecodeCb(int reset_generation) {
  // Both of the following conditions are necessary because:
  //  * In an error state, the reset generations will match but |eos_decode_cb_|
  //    will be aborted.
  //  * After a Reset(), the reset generations won't match, but we might already
  //    have a new |eos_decode_cb_| for the new generation.
  if (reset_generation == reset_generation_ && eos_decode_cb_)
    std::move(eos_decode_cb_).Run(DecoderStatus::Codes::kOk);
}

void MediaCodecVideoDecoder::ForwardVideoFrame(
    int reset_generation,
    std::unique_ptr<ScopedAsyncTrace> async_trace,
    base::TimeTicks started_at,
    scoped_refptr<VideoFrame> frame) {
  DVLOG(3) << __func__ << " : "
           << (frame ? frame->AsHumanReadableString() : "null");

  // No |frame| indicates an error creating it.
  if (!frame) {
    DLOG(ERROR) << __func__ << " |frame| is null";
    EnterTerminalState(State::kError, "Could not create VideoFrame");
    return;
  }

  // Attach the HDR metadata if the color space got this far and is still an HDR
  // color space.  Note that it might be converted to something else along the
  // way, often sRGB.  In that case, don't confuse things with HDR metadata.
  if (frame->ColorSpace().IsHDR() && decoder_config_.hdr_metadata()) {
    frame->set_hdr_metadata(decoder_config_.hdr_metadata());
  }

  if (reset_generation == reset_generation_) {
    // TODO(liberato): We might actually have a SW decoder.  Consider setting
    // this to false if so, especially for higher bitrates.
    frame->metadata().power_efficient = true;
    output_cb_.Run(std::move(frame));
  }
}

// Our Reset() provides a slightly stronger guarantee than VideoDecoder does.
// After |closure| runs:
// 1) no VideoFrames from before the Reset() will be output, and
// 2) no DecodeCBs (including EOS) from before the Reset() will be run.
void MediaCodecVideoDecoder::Reset(base::OnceClosure closure) {
  DVLOG(2) << __func__;
  DCHECK(!reset_cb_);
  reset_generation_++;
  reset_cb_ = std::move(closure);
  CancelPendingDecodes(DecoderStatus::Codes::kAborted);
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
  // TODO(watk): Strongly consider blocking VP8 (or specific MediaCodecs)
  // instead. Draining is responsible for a lot of complexity.
  if (decoder_config_.codec() != VideoCodec::kVP8 || !codec_ ||
      codec_->IsFlushed() || codec_->IsDrained() || using_async_api_) {
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
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
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

void MediaCodecVideoDecoder::EnterTerminalState(State state,
                                                const char* reason) {
  DVLOG(2) << __func__ << " " << static_cast<int>(state) << " " << reason;
  MEDIA_LOG(INFO, media_log_) << "Entering Terminal State: " << reason;

  state_ = state;
  DCHECK(InTerminalState());

  // Cancel pending codec creation.
  codec_allocator_weak_factory_.InvalidateWeakPtrs();
  pump_codec_timer_.Stop();
  ReleaseCodec();
  target_surface_bundle_ = nullptr;
  texture_owner_bundle_ = nullptr;
  if (state == State::kError)
    CancelPendingDecodes({DecoderStatus::Codes::kFailed, reason});
  if (drain_type_)
    OnCodecDrained();
}

bool MediaCodecVideoDecoder::InTerminalState() {
  return state_ == State::kSurfaceDestroyed || state_ == State::kError;
}

void MediaCodecVideoDecoder::CancelPendingDecodes(DecoderStatus status) {
  for (auto& pending_decode : pending_decodes_)
    std::move(pending_decode.decode_cb).Run(status);
  pending_decodes_.clear();
  if (eos_decode_cb_)
    std::move(eos_decode_cb_).Run(status);
}

void MediaCodecVideoDecoder::ReleaseCodec() {
  if (!codec_)
    return;
  auto pair = codec_->TakeCodecSurfacePair();
  codec_ = nullptr;
  codec_allocator_->ReleaseMediaCodec(
      std::move(pair.first),
      base::BindOnce(
          &base::SequencedTaskRunner::ReleaseSoon<CodecSurfaceBundle>,
          base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
          std::move(pair.second)));
}

AndroidOverlayFactoryCB MediaCodecVideoDecoder::CreateOverlayFactoryCb() {
  if (!overlay_factory_cb_ || !overlay_info_.HasValidRoutingToken())
    return AndroidOverlayFactoryCB();

  return base::BindRepeating(overlay_factory_cb_, *overlay_info_.routing_token);
}

VideoDecoderType MediaCodecVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kMediaCodec;
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
  return base::BindPostTaskToCurrentDefault(base::BindRepeating(
      [](base::WeakPtr<MediaCodecVideoDecoder> mcvd,
         CodecSurfaceBundle::ScheduleLayoutCB layout_cb,
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
  return codec_ && codec_->SurfaceBundle() &&
         codec_->SurfaceBundle()->overlay();
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
