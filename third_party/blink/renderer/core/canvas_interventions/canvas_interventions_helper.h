// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_

#include "third_party/blink/public/common/fingerprinting_protection/noise_token.h"
#include "third_party/blink/public/mojom/fingerprinting_protection/canvas_interventions.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

inline constexpr std::string_view kNoiseReasonMetricName =
    "FingerprintingProtection.CanvasNoise.InterventionReason";
inline constexpr std::string_view kNoiseDurationMetricName =
    "FingerprintingProtection.CanvasNoise.NoiseDuration";
inline constexpr std::string_view kCanvasSizeMetricName =
    "FingerprintingProtection.CanvasNoise.NoisedCanvasSize";
inline constexpr std::string_view kCanvasOperationMetricName =
    "FingerprintingProtection.CanvasNoise.OperationTriggered";
inline constexpr std::string_view kCanvasNoiseReadbacksPerContextMetricName =
    "FingerprintingProtection.CanvasNoise.NoisedReadbacksPerContext";

inline constexpr std::string_view kBlockCanvasReadbackErrorMessage =
    "https://issues.chromium.org/issues/"
    "new?component=1456351&title=Breakage%20due%20to%20blocked%20canvas"
    "%20readback. The feature can be disabled through "
    "chrome://flags/#enable-block-canvas-readback";

enum class CanvasNoiseReason {
  kAllConditionsMet = 0,
  kNoRenderContext = 1,  // Deprecated; this is now implied by the trigger.
  kNoTrigger = 2,
  kNo2d = 4,   // Deprecated; this is now implied by the trigger.
  kNoGpu = 8,  // Deprecated; this is now implied by the trigger.
  kNotEnabledInMode = 16,
  kNoExecutionContext = 32,
  kMaxValue = kNoExecutionContext
};

inline constexpr CanvasNoiseReason operator|(CanvasNoiseReason a,
                                             CanvasNoiseReason b) {
  return static_cast<CanvasNoiseReason>(static_cast<int>(a) |
                                        static_cast<int>(b));
}
inline constexpr CanvasNoiseReason& operator|=(CanvasNoiseReason& a,
                                               CanvasNoiseReason b) {
  a = a | b;
  return a;
}

class CORE_EXPORT CanvasInterventionsHelper
    : public GarbageCollected<CanvasInterventionsHelper>,
      public Supplement<ExecutionContext>,
      public ExecutionContextLifecycleObserver,
      public mojom::blink::CanvasNoiseTokenUpdater {
 public:
  enum class CanvasInterventionType {
    kNone,
    kNoise,
  };

  static const char kSupplementName[];

  static CanvasInterventionsHelper* From(ExecutionContext* execution_context);

  explicit CanvasInterventionsHelper(ExecutionContext& execution_context);

  // If allowed, performs noising on a copy of the snapshot StaticBitmapImage
  // and returns the noised snapshot, otherwise it will return the original
  // inputted snapshot.
  static bool MaybeNoiseSnapshot(ExecutionContext* execution_context,
                                 scoped_refptr<StaticBitmapImage>& snapshot);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(receiver_);
    Supplement<ExecutionContext>::Trace(visitor);
    ExecutionContextLifecycleObserver::Trace(visitor);
  }

  void IncrementNoisedCanvasReadbacks() { ++num_noised_canvas_readbacks_; }

  void ContextDestroyed() override;

  void Bind(mojo::PendingReceiver<CanvasNoiseTokenUpdater> pending_receiver);

 private:
  // mojom::blink::CanvasNoiseTokenUpdater overrides:
  void OnTokenReceived(std::optional<NoiseToken> token) override;

  uint32_t num_noised_canvas_readbacks_ = 0;

  HeapMojoReceiver<mojom::blink::CanvasNoiseTokenUpdater,
                   CanvasInterventionsHelper>
      receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_
