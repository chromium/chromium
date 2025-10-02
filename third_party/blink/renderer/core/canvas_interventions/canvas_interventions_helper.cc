// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_helper.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/canvas_interventions/noise_hash.h"
#include "third_party/blink/renderer/core/canvas_interventions/noise_helper.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/graphics/canvas_high_entropy_op_type.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/skia_span_util.h"

namespace blink {

namespace {

constexpr const char kCanvasOperationMetricPrefix[] =
    "FingerprintingProtection.CanvasNoise.OperationTriggered.";
constexpr const char kCanvasNoiseReadbacksPerContextMetricPrefix[] =
    "FingerprintingProtection.CanvasNoise.NoisedReadbacksPerContext.";

std::string_view GetContextTypeForMetrics(ExecutionContext* execution_context) {
  if (execution_context->IsWindow()) {
    return "Window";
  }
  if (execution_context->IsDedicatedWorkerGlobalScope()) {
    return "DedicatedWorker";
  }
  if (execution_context->IsSharedWorkerGlobalScope()) {
    return "SharedWorker";
  }
  if (execution_context->IsServiceWorkerGlobalScope()) {
    return "ServiceWorker";
  }
  return "Other";
}

// Returns true when all criteria to apply noising are met. Currently this
// entails that
//   1) a triggering operation was made on the canvas, implying it was made on
//      an accelerated 2d context
//   2) the CanvasInterventions RuntimeEnabledFeature is enabled
bool ShouldApplyNoise(HighEntropyCanvasOpType canvas_operations,
                      ExecutionContext* execution_context) {
  CanvasNoiseReason noise_reason = CanvasNoiseReason::kAllConditionsMet;
  if (canvas_operations == HighEntropyCanvasOpType::kNone) {
    noise_reason |= CanvasNoiseReason::kNoTrigger;
  }
  if (!execution_context) {
    noise_reason |= CanvasNoiseReason::kNoExecutionContext;
  }
  // Check if all heuristics have matched so far (excluding whether the feature
  // is enabled).
  if (noise_reason == CanvasNoiseReason::kAllConditionsMet) {
    UseCounter::Count(execution_context,
                      WebFeature::kCanvasReadbackNoiseMatchesHeuristics);
  }
  if (execution_context && !execution_context->CanvasNoiseToken().has_value()) {
    noise_reason |= CanvasNoiseReason::kNotEnabledInMode;
  }

  // When all conditions are met, none of the other reasons are possible.
  constexpr int exclusive_max = static_cast<int>(CanvasNoiseReason::kMaxValue)
                                << 1;

  UMA_HISTOGRAM_EXACT_LINEAR(kNoiseReasonMetricName,
                             static_cast<int>(noise_reason), exclusive_max);

  return noise_reason == CanvasNoiseReason::kAllConditionsMet;
}
}  // namespace

// static
const char CanvasInterventionsHelper::kSupplementName[] =
    "CanvasInterventionsHelper";

// static
bool CanvasInterventionsHelper::MaybeNoiseSnapshot(
    ExecutionContext* execution_context,
    scoped_refptr<StaticBitmapImage>& snapshot) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  CHECK(snapshot);

  HighEntropyCanvasOpType high_entropy_canvas_op_types =
      snapshot->HighEntropyCanvasOpTypes();
  if (!ShouldApplyNoise(high_entropy_canvas_op_types, execution_context)) {
    return false;
  }

  // Use kUnpremul_SkAlphaType as alpha type as we are changing the pixel values
  // of all channels, including the alpha channel.
  auto info = SkImageInfo::Make(
      snapshot->GetSize().width(), snapshot->GetSize().height(),
      viz::ToClosestSkColorType(snapshot->GetSharedImageFormat()),
      kUnpremul_SkAlphaType, snapshot->GetColorSpace().ToSkColorSpace());
  SkBitmap bm;
  if (!bm.tryAllocPixels(info)) {
    return false;
  }

  // Copy the original pixels from snapshot to the modifiable SkPixmap. SkBitmap
  // should already allocate the correct amount of pixels, so this shouldn't
  // fail because of memory allocation.
  auto pixmap_to_noise = bm.pixmap();
  PaintImage paint_image = snapshot->PaintImageForCurrentFrame();
  if (!paint_image.readPixels(bm.info(), pixmap_to_noise.writable_addr(),
                              bm.rowBytes(), 0, 0)) {
    return false;
  }

  base::span<uint8_t> modify_pixels =
      gfx::SkPixmapToWritableSpan(pixmap_to_noise);

  // Guaranteed to have a value, as per |ShouldApplyNoise|.
  auto token_hash = NoiseHash(execution_context->CanvasNoiseToken().value());
  NoisePixels(token_hash, modify_pixels, pixmap_to_noise.width(),
              pixmap_to_noise.height());

  auto noised_image = bm.asImage();
  snapshot = blink::UnacceleratedStaticBitmapImage::Create(
      std::move(noised_image), snapshot->Orientation());

  constexpr int canvas_op_exclusive_max =
      static_cast<int>(HighEntropyCanvasOpType::kMaxValue) << 1;
  UMA_HISTOGRAM_EXACT_LINEAR(kCanvasOperationMetricName,
                             static_cast<int>(high_entropy_canvas_op_types),
                             canvas_op_exclusive_max);
  base::UmaHistogramExactLinear(
      base::StrCat({kCanvasOperationMetricPrefix,
                    GetContextTypeForMetrics(execution_context)}),
      static_cast<int>(high_entropy_canvas_op_types), canvas_op_exclusive_max);

  AuditsIssue::ReportUserReidentificationCanvasNoisedIssue(
      CaptureSourceLocation(execution_context), execution_context);

  execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kIntervention,
      mojom::blink::ConsoleMessageLevel::kInfo,
      "Noise was added to a canvas readback. If this has caused breakage, "
      "please file a bug at https://issues.chromium.org/issues/"
      "new?component=1456351&title=Canvas%20noise%20breakage. This "
      "feature can be disabled through chrome://flags/#enable-canvas-noise"));

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(kNoiseDurationMetricName,
                                          elapsed_time, base::Microseconds(50),
                                          base::Milliseconds(100), 100);
  UMA_HISTOGRAM_COUNTS_1M(kCanvasSizeMetricName,
                          pixmap_to_noise.width() * pixmap_to_noise.height());
  UseCounter::Count(execution_context, WebFeature::kCanvasReadbackNoise);
  auto* helper = CanvasInterventionsHelper::From(execution_context);
  helper->IncrementNoisedCanvasReadbacks();

  return true;
}

// static
CanvasInterventionsHelper* CanvasInterventionsHelper::From(
    ExecutionContext* context) {
  CanvasInterventionsHelper* helper =
      Supplement<ExecutionContext>::From<CanvasInterventionsHelper>(context);
  if (!helper) {
    helper = MakeGarbageCollected<CanvasInterventionsHelper>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, helper);
  }
  return helper;
}

CanvasInterventionsHelper::CanvasInterventionsHelper(
    ExecutionContext& execution_context)
    : Supplement<ExecutionContext>(execution_context),
      ExecutionContextLifecycleObserver(&execution_context),
      receiver_(this, &execution_context) {}

void CanvasInterventionsHelper::ContextDestroyed() {
  // CanvasInterventionsHelper will be created for every ExecutionContext,
  // which, only perhaps a subset will have a noised canvas. We should not
  // record any ExecutionContexts that did not have a noised canvas as that
  // would significantly bloat the 0 increments for this UMA.
  if (num_noised_canvas_readbacks_ == 0) {
    return;
  }
  UMA_HISTOGRAM_COUNTS_100(kCanvasNoiseReadbacksPerContextMetricName,
                           num_noised_canvas_readbacks_);
  base::UmaHistogramCounts100(
      base::StrCat({kCanvasNoiseReadbacksPerContextMetricPrefix,
                    GetContextTypeForMetrics(GetExecutionContext())}),
      num_noised_canvas_readbacks_);
}

void CanvasInterventionsHelper::Bind(
    mojo::PendingReceiver<CanvasNoiseTokenUpdater> pending_receiver) {
  receiver_.Bind(
      std::move(pending_receiver),
      GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault));
}

void CanvasInterventionsHelper::OnTokenReceived(
    std::optional<NoiseToken> token) {
  GetExecutionContext()->SetCanvasNoiseToken(token);
}

}  // namespace blink
