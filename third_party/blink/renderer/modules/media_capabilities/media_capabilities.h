// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_MEDIA_CAPABILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_MEDIA_CAPABILITIES_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/base/video_codecs.h"  // for media::VideoCodecProfile
#include "media/base/video_color_space.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom-blink.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_configuration.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_decoding_info_handler.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_encoding_info_handler.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class MediaCapabilitiesDecodingInfo;
class MediaCapabilitiesInfo;
class MediaDecodingConfiguration;
class MediaEncodingConfiguration;
class MediaKeySystemAccess;
class NavigatorBase;
class ScriptState;

// =============================================================================
// MediaCapabilities Pipeline Architecture: Non-WebRTC vs. WebRTC
// =============================================================================
//
// 1. NON-WEBRTC PIPELINE (type: 'file' | 'media-source')
// -----------------------------------------------------------------------------
// • Support Model (Two-Stage):
//   a) Coarse Pre-Check: Checks StreamParserFactory (MSE) or
//      media::IsDecoderSupportedVideoType() (file) upfront. Validates codec,
//      profile, color space, and HDR metadata, but has NO knowledge of stream
//      dimensions (width/height) or GPU device limits.
//   b) Fine-Grained GPU Check: GpuVideoAcceleratorFactories verifies whether
//      the hardware decoder can handle the specific resolution.
//
// • Execution Flow (Parallel):
//   Fires two asynchronous queries concurrently:
//   1. decode_history_service_->GetPerfInfo() -> Queries VideoDecodePerfHistory
//      database for `smooth` and baseline `powerEfficient`.
//   2. GetGpuFactoriesSupport() -> Queries GPU process with exact resolution
//      to determine hardware decode support (sets `powerEfficient`).
//
// • Software Fallback & Hardware-Only Codecs:
//   If GPU factories report `powerEfficient == false` (or fail to respond):
//   - Built-in codecs (VP9, AV1, H.264 w/ FFmpeg) have CPU software decoders,
//     so playback succeeds in software -> `supported` remains true.
//   - Non-builtin codecs (HEVC, Dolby Vision) have NO software decoders in the
//     binary. Lack of GPU support means playback will fail -> `supported` is
//     flipped to false.
//
// • Worker & EME Scopes:
//   In Web Workers or EME (where UseGpuFactoriesForPowerEfficient() == false),
//   GpuFactories cannot be queried. The pipeline relies on DB heuristics and
//   preserves `supported = true` to avoid false negatives for hardware codecs.
//
// • Timeout / Fallback Rule:
//   - powerEfficient = is_gpu_factories_supported.value_or(false)
//   - supported      = is_power_efficient || is_builtin_video_codec
//   - smooth         = db_is_smooth.value_or(false)
//
// =============================================================================
// 2. WEBRTC PIPELINE (type: 'webrtc')
// -----------------------------------------------------------------------------
// • Support Model (Unified / Single-Stage):
//   No coarse pre-check. Capabilities are delegated directly to
//   Webrtc(Decoding|Encoding)InfoHandler, which queries WebRTC's internal
//   software and hardware factories atomically with resolution and scalability.
//
// • Execution Flow (Sequential / In Series):
//   Queries CANNOT run in parallel because the database lookup requires the
//   hardware acceleration status:
//   1. Step 1: Handler evaluates codec support -> returns `is_supported` and
//      `is_power_efficient` synchronously or via callback.
//   2. Step 2: `is_power_efficient` is assigned to
//      WebrtcPredictionFeatures::hardware_accelerated.
//   3. Step 3: webrtc_history_service_->GetPerfInfo() queries the DB for
//      `smooth`.
//
// • Software Fallback:
//   WebRTC codec factories natively manage their own software vs. hardware
//   selection (e.g. libvpx/OpenH264 fallback vs. RTCVideoDecoderFactory).
//
// • Timeout / Fallback Rule:
//   - If the DB times out in Step 3, the known `is_supported` and
//     `is_power_efficient` from Step 1 are preserved, while `smooth` defaults
//     to false.
//   - If the handler itself timed out before Step 1, support falls back to
//     handler->IsSoftware(Decoder|Encoder)Supported().
// =============================================================================
class MODULES_EXPORT MediaCapabilities final
    : public ScriptWrappable,
      public Supplement<NavigatorBase> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static constexpr base::TimeDelta kMediaCapabilitiesQueryTimeout =
      base::Seconds(10);

  static const char kWebrtcDecodeSmoothIfPowerEfficientParamName[];
  static const char kWebrtcEncodeSmoothIfPowerEfficientParamName[];

  static const char kSupplementName[];

  enum class QueryType { kDecoding, kWebrtcDecoding, kWebrtcEncoding };

  // Getter for navigator.mediaCapabilities
  static MediaCapabilities* mediaCapabilities(NavigatorBase&);

  explicit MediaCapabilities(NavigatorBase&);

  void Trace(blink::Visitor* visitor) const override;

  ScriptPromise<MediaCapabilitiesDecodingInfo> decodingInfo(
      ScriptState*,
      const MediaDecodingConfiguration*,
      ExceptionState&);
  ScriptPromise<MediaCapabilitiesInfo> encodingInfo(
      ScriptState*,
      const MediaEncodingConfiguration*,
      ExceptionState&);

 private:
  // Stores pending callback state from and intermediate prediction values while
  // we wait for all predictions to arrive.
  class PendingCallbackState : public GarbageCollected<PendingCallbackState> {
   public:
    PendingCallbackState(ScriptPromiseResolverBase* resolver,
                         MediaKeySystemAccess* access,
                         const base::TimeTicks& request_time,
                         QueryType query_type = QueryType::kDecoding);
    virtual void Trace(blink::Visitor* visitor) const;

    Member<ScriptPromiseResolverBase> resolver;
    Member<MediaKeySystemAccess> key_system_access;
    std::optional<bool> is_supported;
    std::optional<bool> is_bad_window_prediction_smooth;
    std::optional<bool> is_nnr_prediction_smooth;
    std::optional<bool> db_is_smooth;
    std::optional<bool> db_is_power_efficient;
    std::optional<bool> is_gpu_factories_supported;
    std::optional<bool> is_builtin_video_codec;
    base::TimeTicks request_time;
    QueryType query_type;

    bool IsWebrtc() const {
      return query_type == QueryType::kWebrtcDecoding ||
             query_type == QueryType::kWebrtcEncoding;
    }
  };

  FRIEND_TEST_ALL_PREFIXES(MediaCapabilitiesWebrtcTests,
                           DecodePowerEfficientIsSmooth);
  FRIEND_TEST_ALL_PREFIXES(MediaCapabilitiesWebrtcTests,
                           DecodeOverridePowerEfficientIsSmooth);
  FRIEND_TEST_ALL_PREFIXES(MediaCapabilitiesWebrtcTests,
                           EncodePowerEfficientIsSmooth);
  FRIEND_TEST_ALL_PREFIXES(MediaCapabilitiesWebrtcTests,
                           EncodeOverridePowerEfficientIsSmooth);

  // Lazily binds to the VideoDecodePerfHistory service. Returns whether it was
  // successful. Returns true if it was already bound.
  bool EnsurePerfHistoryService(ExecutionContext*);

  // Lazily binds to the WebrtcVideoPerfHistory service. Returns whether it was
  // successful. Returns true if it was already bound.
  bool EnsureWebrtcPerfHistoryService(ExecutionContext* execution_context);

  ScriptPromise<MediaCapabilitiesDecodingInfo> GetEmeSupport(
      ScriptState*,
      media::VideoCodec,
      media::VideoCodecProfile,
      media::VideoColorSpace,
      const MediaDecodingConfiguration*,
      const base::TimeTicks& request_time,
      ExceptionState&);
  // Gets perf info from VideoDecodePerrHistory DB.
  void GetPerfInfo(media::VideoCodec,
                   media::VideoCodecProfile,
                   media::VideoColorSpace,
                   const MediaDecodingConfiguration*,
                   const base::TimeTicks& request_time,
                   ScriptPromiseResolver<MediaCapabilitiesDecodingInfo>*,
                   MediaKeySystemAccess*);

  // Query media::GpuVideoAcceleratorFactories for support of hardware
  // accelerate decode. Only called when |UseGpuFactoriesForPowerEfficient()|
  // is true.
  void GetGpuFactoriesSupport(int callback_id,
                              media::VideoCodec video_codec,
                              media::VideoCodecProfile video_profile,
                              media::VideoColorSpace,
                              const MediaDecodingConfiguration*);

  // Callback for perf info from the VideoDecodePerfHistory service.
  void OnPerfHistoryInfo(int callback_id,
                         bool is_smooth,
                         bool is_power_efficient);

  // Callback for GetGpuFactoriesSupport().
  void OnGpuFactoriesSupport(int callback_id,
                             bool is_supported,
                             media::VideoCodec video_codec);

  // Resolves the callback with associated |callback_id| and removes it from the
  // |pending_callback_map_|.
  void ResolveCallbackIfReady(int callback_id);

  void OnWebrtcSupportInfo(
      int callback_id,
      media::mojom::blink::WebrtcPredictionFeaturesPtr features,
      float frames_per_second,
      QueryType,
      bool is_supported,
      bool is_power_efficient);

  void OnWebrtcPerfHistoryInfo(int callback_id, QueryType, bool is_smooth);

  void OnCallbackTimeout(int callback_id);
  void OnServiceDisconnected(bool is_webrtc);

  int InsertCallbackAndScheduleTimeout(ScriptPromiseResolverBase* resolver,
                                       MediaKeySystemAccess* access,
                                       const base::TimeTicks& request_time,
                                       QueryType query_type);
  void ResolveWebrtcCallback(int callback_id,
                             bool is_supported,
                             bool is_power_efficient,
                             bool is_smooth);

  // Creates a new (incremented) callback ID from |last_callback_id_| for
  // mapping in |pending_cb_map_|.
  int CreateCallbackId();

  void set_webrtc_decoding_info_handler_for_test(
      WebrtcDecodingInfoHandler* handler) {
    webrtc_decoding_info_handler_for_test_ = handler;
  }

  void set_webrtc_encoding_info_handler_for_test(
      WebrtcEncodingInfoHandler* handler) {
    webrtc_encoding_info_handler_for_test_ = handler;
  }

  HeapMojoRemote<media::mojom::blink::VideoDecodePerfHistory>
      decode_history_service_;

  HeapMojoRemote<media::mojom::blink::WebrtcVideoPerfHistory>
      webrtc_history_service_;

  // Holds the last key for callbacks in the map below. Incremented for each
  // usage.
  int last_callback_id_ = 0;

  // Maps a callback ID to state for pending callbacks.
  HeapHashMap<int, Member<PendingCallbackState>> pending_cb_map_;

  // Makes it possible to override the WebrtcDecodingInfoHandler in tests.
  raw_ptr<WebrtcDecodingInfoHandler> webrtc_decoding_info_handler_for_test_ =
      nullptr;

  // Makes it possible to override the WebrtcEncodingInfoHandler in tests.
  raw_ptr<WebrtcEncodingInfoHandler> webrtc_encoding_info_handler_for_test_ =
      nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_MEDIA_CAPABILITIES_H_
