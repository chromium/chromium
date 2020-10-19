// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_MEDIA_CAPABILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_MEDIA_CAPABILITIES_H_

#include "media/base/video_codecs.h"  // for media::VideoCodecProfile
#include "media/learning/mojo/public/cpp/mojo_learning_task_controller.h"
#include "media/learning/mojo/public/mojom/learning_task_controller.mojom-blink.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom-blink.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_configuration.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class MediaDecodingConfiguration;
class MediaEncodingConfiguration;
class MediaKeySystemAccess;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT MediaCapabilities final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kLearningBadWindowThresholdParamName[];
  static const char kLearningNnrThresholdParamName[];

  explicit MediaCapabilities(ExecutionContext* context);

  void Trace(blink::Visitor* visitor) const override;

  ScriptPromise decodingInfo(ScriptState*,
                             const MediaDecodingConfiguration*,
                             ExceptionState&);
  ScriptPromise encodingInfo(ScriptState*, const MediaEncodingConfiguration*);

 private:
  // Stores pending callback state from and intermediate prediction values while
  // we wait for all predictions to arrive.
  class PendingCallbackState : public GarbageCollected<PendingCallbackState> {
   public:
    PendingCallbackState(ScriptPromiseResolver* resolver,
                         MediaKeySystemAccess* access,
                         const base::TimeTicks& request_time,
                         base::Optional<IdentifiableToken> input_token);
    virtual void Trace(blink::Visitor* visitor) const;

    Member<ScriptPromiseResolver> resolver;
    Member<MediaKeySystemAccess> key_system_access;
    base::Optional<bool> is_bad_window_prediction_smooth;
    base::Optional<bool> is_nnr_prediction_smooth;
    base::Optional<bool> db_is_smooth;
    base::Optional<bool> db_is_power_efficient;
    base::Optional<bool> is_gpu_factories_supported;
    base::TimeTicks request_time;
    base::Optional<IdentifiableToken> input_token;
  };

  // Lazily binds remote LearningTaskControllers for ML smoothness predictions
  // and returns whether binding succeeds. Returns true if it was already bound.
  bool EnsureLearningPredictors(ExecutionContext*);

  // Lazily binds to the VideoDecodePerfHistory service. Returns whether it was
  // successful. Returns true if it was already bound.
  bool EnsurePerfHistoryService(ExecutionContext*);

  ScriptPromise GetEmeSupport(ScriptState*,
                              media::VideoCodec,
                              media::VideoCodecProfile,
                              media::VideoColorSpace,
                              const MediaDecodingConfiguration*,
                              const base::TimeTicks& request_time,
                              ExceptionState&);
  // Gets perf info from VideoDecodePerrHistory DB. Will optionally kick off
  // parallel request to GetPerfInfo_ML() when learning experiment is enabled.
  void GetPerfInfo(media::VideoCodec,
                   media::VideoCodecProfile,
                   media::VideoColorSpace,
                   const MediaDecodingConfiguration*,
                   const base::TimeTicks& request_time,
                   ScriptPromiseResolver*,
                   MediaKeySystemAccess*);

  // Gets ML perf predictions from remote LearingTaskControllers.
  void GetPerfInfo_ML(ExecutionContext* execution_context,
                      int callback_id,
                      media::VideoCodec video_codec,
                      media::VideoCodecProfile video_profile,
                      int width,
                      double framerate);

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

  // Callback for predictions from |bad_window_predictor_|.
  void OnBadWindowPrediction(
      int callback_id,
      const base::Optional<::media::learning::TargetHistogram>& histogram);

  // Callback for predictions from |nnr_predictor_|.
  void OnNnrPrediction(
      int callback_id,
      const base::Optional<::media::learning::TargetHistogram>& histogram);

  // Callback for GetGpuFactoriesSupport().
  void OnGpuFactoriesSupport(int callback_id, bool is_supported);

  // Resolves the callback with associated |callback_id| and removes it from the
  // |pending_callback_map_|.
  void ResolveCallbackIfReady(int callback_id);

  // Creates a new (incremented) callback ID from |last_callback_id_| for
  // mapping in |pending_cb_map_|.
  int CreateCallbackId();

  HeapMojoRemote<media::mojom::blink::VideoDecodePerfHistory,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      decode_history_service_;

  // Connection to a browser-process LearningTaskController for predicting the
  // number of consecutive "bad" dropped frame windows during a playback. See
  // media::SmoothnessHelper.
  HeapMojoRemote<media::learning::mojom::blink::LearningTaskController,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      bad_window_predictor_;

  // Connects to a browser-process LearningTaskController for predicting the
  // number of consecutive non-network re-buffers (NNRs). See
  // media::SmoothnessHelper.
  HeapMojoRemote<media::learning::mojom::blink::LearningTaskController,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      nnr_predictor_;

  // Holds the last key for callbacks in the map below. Incremented for each
  // usage.
  int last_callback_id_ = 0;

  // Maps a callback ID to state for pending callbacks.
  HeapHashMap<int, Member<PendingCallbackState>> pending_cb_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_MEDIA_CAPABILITIES_H_
