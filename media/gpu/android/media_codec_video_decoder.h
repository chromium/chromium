// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MEDIA_CODEC_VIDEO_DECODER_H_
#define MEDIA_GPU_ANDROID_MEDIA_CODEC_VIDEO_DECODER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android/media_crypto_context.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_status.h"
#include "media/base/overlay_info.h"
#include "media/base/scoped_async_trace.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/android/android_video_surface_chooser.h"
#include "media/gpu/android/codec_allocator.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/device_info.h"
#include "media/gpu/android/surface_chooser_helper.h"
#include "media/gpu/android/video_frame_factory.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class MediaLog;
struct SupportedVideoDecoderConfig;

struct PendingDecode {
  static PendingDecode CreateEos();
  PendingDecode(scoped_refptr<DecoderBuffer> buffer,
                VideoDecoder::DecodeCB decode_cb);

  PendingDecode(const PendingDecode&) = delete;
  PendingDecode& operator=(const PendingDecode&) = delete;

  PendingDecode(PendingDecode&& other);

  ~PendingDecode();

  scoped_refptr<DecoderBuffer> buffer;
  VideoDecoder::DecodeCB decode_cb;
};

// An Android VideoDecoder that delegates to MediaCodec.
//
// This decoder initializes in two stages. Low overhead initialization is done
// eagerly in Initialize(), but the rest is done lazily and is kicked off by the
// first Decode() (see StartLazyInit()). We do this because there are cases in
// our media pipeline where we'll initialize a decoder but never use it
// (e.g., MSE with no media data appended), and if we eagerly allocator decoder
// resources, like MediaCodecs and TextureOwners, we will block other
// playbacks that need them.
// TODO: Lazy initialization should be handled at a higher layer of the media
// stack for both simplicity and cross platform support.
class MEDIA_GPU_EXPORT MediaCodecVideoDecoder final
    : public VideoDecoder,
      public gpu::RefCountedLockHelperDrDc {
 public:
  static std::vector<SupportedVideoDecoderConfig> GetSupportedConfigs();

  MediaCodecVideoDecoder(const MediaCodecVideoDecoder&) = delete;
  MediaCodecVideoDecoder& operator=(const MediaCodecVideoDecoder&) = delete;

  ~MediaCodecVideoDecoder() override;
  static void DestroyAsync(std::unique_ptr<MediaCodecVideoDecoder>);

  static std::unique_ptr<VideoDecoder> Create(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      std::unique_ptr<MediaLog> media_log,
      DeviceInfo* device_info,
      CodecAllocator* codec_allocator,
      std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
      AndroidOverlayMojoFactoryCB overlay_factory_cb,
      RequestOverlayInfoCB request_overlay_info_cb,
      std::unique_ptr<VideoFrameFactory> video_frame_factory,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  // VideoDecoder implementation:
  VideoDecoderType GetDecoderType() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

 private:
  // The test has access for PumpCodec() and the constructor.
  friend class MediaCodecVideoDecoderTest;

  MediaCodecVideoDecoder(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      std::unique_ptr<MediaLog> media_log,
      DeviceInfo* device_info,
      CodecAllocator* codec_allocator,
      std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
      AndroidOverlayMojoFactoryCB overlay_factory_cb,
      RequestOverlayInfoCB request_overlay_info_cb,
      std::unique_ptr<VideoFrameFactory> video_frame_factory,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  // Set up |cdm_context| as part of initialization.  Guarantees that |init_cb|
  // will be called depending on the outcome, though not necessarily before this
  // function returns.
  void SetCdm(CdmContext* cdm_context, InitCB init_cb);

  // Called when the Cdm provides |media_crypto|.  Will signal |init_cb| based
  // on the result, and set the codec config properly.
  void OnMediaCryptoReady(InitCB init_cb,
                          JavaObjectPtr media_crypto,
                          bool requires_secure_video_codec);

  enum class State {
    // Initializing resources required to create a codec.
    kInitializing,
    // Initialization has completed and we're running. This is the only state
    // in which |codec_| might be non-null. If |codec_| is null, a codec
    // creation is pending.
    kRunning,
    // A fatal error occurred. A terminal state.
    kError,
    // The output surface was destroyed, but SetOutputSurface() is not supported
    // by the device. In this case the consumer is responsible for destroying us
    // soon, so this is terminal state but not a decode error.
    kSurfaceDestroyed
  };

  enum class DrainType { kForReset, kForDestroy };

  // Finishes initialization.
  void StartLazyInit();
  void OnVideoFrameFactoryInitialized(
      scoped_refptr<gpu::TextureOwner> texture_owner);

  // Callback for the CDM to notify |this|. Resets |waiting_for_key_| to false,
  // indicating that MediaCodec might now accept buffers.
  void OnCdmContextEvent(CdmContext::Event event);

  // Updates |surface_chooser_| with the new overlay info.
  void OnOverlayInfoChanged(const OverlayInfo& overlay_info);
  void OnSurfaceChosen(std::unique_ptr<AndroidOverlay> overlay);
  void OnSurfaceDestroyed(AndroidOverlay* overlay);

  // Whether we have a codec and its surface is not equal to
  // |target_surface_bundle_|.
  bool SurfaceTransitionPending();

  // Sets |codecs_|'s output surface to |target_surface_bundle_|.
  void TransitionToTargetSurface();

  // Creates a codec asynchronously.
  void CreateCodec();

  // Trampoline helper which ensures correct release of MediaCodecBridge and
  // CodecSurfaceBundle even if this class goes away.
  static void OnCodecConfiguredInternal(
      base::WeakPtr<MediaCodecVideoDecoder> weak_this,
      CodecAllocator* codec_allocator,
      scoped_refptr<CodecSurfaceBundle> surface_bundle,
      std::unique_ptr<MediaCodecBridge> codec);
  void OnCodecConfigured(scoped_refptr<CodecSurfaceBundle> surface_bundle,
                         std::unique_ptr<MediaCodecBridge> media_codec);

  // Flushes the codec, or if flush() is not supported, releases it and creates
  // a new one.
  void FlushCodec();

  // Attempts to queue input and dequeue output from the codec.
  void PumpCodec();
  bool QueueInput();
  bool DequeueOutput();

  // Runs |eos_decode_cb_| if it's valid and |reset_generation| matches
  // |reset_generation_|.
  void RunEosDecodeCb(int reset_generation);

  // Forwards |frame| via |output_cb_| if |reset_generation| matches
  // |reset_generation_|.  |async_trace| is the (optional) scoped trace that
  // started when we dequeued the corresponding output buffer.  |started_at| is
  // the wall clock time at which we dequeued the output buffer.
  void ForwardVideoFrame(int reset_generation,
                         std::unique_ptr<ScopedAsyncTrace> async_trace,
                         base::TimeTicks started_at,
                         scoped_refptr<VideoFrame> frame);

  // Starts draining the codec by queuing an EOS if required. It skips the drain
  // if possible.
  void StartDrainingCodec(DrainType drain_type);
  void OnCodecDrained();
  void CancelPendingDecodes(DecoderStatus status);

  // Sets |state_| and does common teardown for the terminal states. |state_|
  // must be either kSurfaceDestroyed or kError.  |reason| will be logged to
  // |media_log_| as an info event ("error" indicates that playback will stop,
  // but we don't know that the renderer will do that).
  void EnterTerminalState(State state, DecoderStatus reason);
  bool InTerminalState();

  // Releases |codec_| if it's not null.
  void ReleaseCodec();

  // Return true if we have a codec that's outputting to an overlay.
  bool IsUsingOverlay() const;

  // Notify us about a promotion hint.
  void NotifyPromotionHint(PromotionHintAggregator::Hint hint);

  // Update |cached_frame_information_|.
  void CacheFrameInformation();

  // Creates an overlay factory cb based on the value of overlay_info_.
  AndroidOverlayFactoryCB CreateOverlayFactoryCb();

  // Create a callback that will handle promotion hints, and set the overlay
  // position if required.
  PromotionHintAggregator::NotifyPromotionHintCB CreatePromotionHintCB();

  // Returns true if the MediaCodec must be reallocated due to an increase in
  // resolution.
  bool CodecNeedsReallocation(int new_width);

  std::vector<SupportedVideoDecoderConfig> GetSupportedConfigsInternal();

  std::unique_ptr<MediaLog> media_log_;

  State state_ = State::kInitializing;

  // Whether initialization still needs to be done on the first decode call.
  bool lazy_init_pending_ = true;
  base::circular_deque<PendingDecode> pending_decodes_;

  // Whether we've seen MediaCodec return MediaCodecResult::Codes::kNoKey
  // indicating that the corresponding key was not set yet, and MediaCodec will
  // not accept buffers until OnCdmContextEvent() is called with
  // kHasAdditionalUsableKey.
  bool waiting_for_key_ = false;

  // The reason for the current drain operation if any.
  std::optional<DrainType> drain_type_;

  // The current reset cb if a Reset() is in progress.
  base::OnceClosure reset_cb_;

  // A generation counter that's incremented every time Reset() is called.
  int reset_generation_ = 0;

  // The EOS decode cb for an EOS currently being processed by the codec. Called
  // when the EOS is output.
  DecodeCB eos_decode_cb_;

  OutputCB output_cb_;
  WaitingCB waiting_cb_;
  VideoDecoderConfig decoder_config_;

  // Codec specific data (SPS and PPS for H264). Some MediaCodecs initialize
  // more reliably if we explicitly pass these (http://crbug.com/649185).
  std::vector<uint8_t> csd0_;
  std::vector<uint8_t> csd1_;

  std::unique_ptr<CodecWrapper> codec_;
  raw_ptr<CodecAllocator> codec_allocator_;

  // The current target surface that |codec_| should be rendering to. It
  // reflects the latest surface choice by |surface_chooser_|. If the codec is
  // configured with some other surface, then a transition is pending. It's
  // non-null from the first surface choice.
  scoped_refptr<CodecSurfaceBundle> target_surface_bundle_;

  // A TextureOwner bundle that is kept for the lifetime of MCVD so that if we
  // have to synchronously switch surfaces we always have one available.
  scoped_refptr<CodecSurfaceBundle> texture_owner_bundle_;

  // A callback for requesting overlay info updates.
  RequestOverlayInfoCB request_overlay_info_cb_;

  // The current overlay info, which possibly specifies an overlay to render to.
  OverlayInfo overlay_info_;

  // Set to true if the display compositor swap is done using SurfaceControl.
  const bool is_surface_control_enabled_;

  // The helper which manages our surface chooser for us.
  SurfaceChooserHelper surface_chooser_helper_;

  // The factory for creating VideoFrames from CodecOutputBuffers.
  std::unique_ptr<VideoFrameFactory> video_frame_factory_;

  // An optional factory callback for creating mojo AndroidOverlays.
  AndroidOverlayMojoFactoryCB overlay_factory_cb_;

  raw_ptr<DeviceInfo> device_info_;
  bool enable_threaded_texture_mailboxes_;

  // Most recently cached frame information, so that we can dispatch it without
  // recomputing it on every frame.  It changes very rarely.
  SurfaceChooserHelper::FrameInformation cached_frame_information_ =
      SurfaceChooserHelper::FrameInformation::NON_OVERLAY_INSECURE;

  // CDM related stuff.

  // Owned by CDM which is external to this decoder.
  raw_ptr<MediaCryptoContext> media_crypto_context_ = nullptr;

  // To keep the CdmContext event callback registered.
  std::unique_ptr<CallbackRegistration> event_cb_registration_;

  // Do we need a hw-secure codec?
  bool requires_secure_codec_ = false;

  // Should we flush the codec on the next decode, and pretend that it is
  // drained currently?  Note that we'll automatically flush if the codec is
  // drained; this flag indicates that we also elided the drain, so the codec is
  // in some random state, possibly with output buffers pending.
  bool deferred_flush_pending_ = false;

  // Should we upgrade the next flush to a full release / reallocation of the
  // codec?  This lets us update our hints to the decoder about the size of the
  // expected video.
  bool deferred_reallocation_pending_ = false;

  // Width, in pixels, of the resolution that we last told the codec about.
  int last_width_ = 0;

  // KEY_MAX_INPUT_SIZE configured for the current codec.
  size_t max_input_size_ = 0;

  // Optional crypto object from the Cdm.
  base::android::ScopedJavaGlobalRef<jobject> media_crypto_;

  // For A/B power testing, this causes all non-L1 content to avoid overlays.
  // This is only for A/B power testing, and can be removed after that.
  // See https://crbug.com/1081346 .
  bool allow_nonsecure_overlays_ = true;

  // If set, then the next call to `CodecConfig()` will be allowed to retry if
  // it fails to get a codec.  This is to work around b/191966399.
  bool should_retry_codec_allocation_ = false;

  // Name of the MediaCodec that was created.
  std::string codec_name_;

  // Enables Block Model (LinearBlock).
  const bool use_block_model_;

  base::WeakPtrFactory<MediaCodecVideoDecoder> weak_factory_{this};
  base::WeakPtrFactory<MediaCodecVideoDecoder> codec_allocator_weak_factory_{
      this};
};

}  // namespace media

namespace std {

// Specialize std::default_delete to call Destroy().
template <>
struct MEDIA_GPU_EXPORT default_delete<media::MediaCodecVideoDecoder>
    : public default_delete<media::VideoDecoder> {};

}  // namespace std

#endif  // MEDIA_GPU_ANDROID_MEDIA_CODEC_VIDEO_DECODER_H_
