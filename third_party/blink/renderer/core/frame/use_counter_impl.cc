/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/use_counter_impl.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

namespace blink {
namespace {
mojom::blink::UseCounterFeatureType ToFeatureType(
    UseCounterImpl::CSSPropertyType type) {
  switch (type) {
    case UseCounterImpl::CSSPropertyType::kDefault:
      return mojom::blink::UseCounterFeatureType::kCssProperty;
    case UseCounterImpl::CSSPropertyType::kAnimation:
      return mojom::blink::UseCounterFeatureType::kAnimatedCssProperty;
  }
}
}  // namespace

UseCounterMuteScope::UseCounterMuteScope(const Element& element)
    : loader_(element.GetDocument().Loader()) {
  if (loader_)
    loader_->GetUseCounter().MuteForInspector();
}

UseCounterMuteScope::~UseCounterMuteScope() {
  if (loader_)
    loader_->GetUseCounter().UnmuteForInspector();
}

UseCounterImpl::UseCounterImpl(Context context, CommitState commit_state)
    : mute_count_(0), context_(context), commit_state_(commit_state) {}

void UseCounterImpl::MuteForInspector() {
  mute_count_++;
}

void UseCounterImpl::UnmuteForInspector() {
  mute_count_--;
}

void UseCounterImpl::RecordMeasurement(WebFeature web_feature,
                                       const LocalFrame& source_frame) {
  if (mute_count_)
    return;

  // PageDestruction is reserved as a scaling factor.
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, web_feature);
  DCHECK_NE(WebFeature::kPageVisits, web_feature);
  DCHECK_GE(WebFeature::kNumberOfFeatures, web_feature);

  if (feature_tracker_.TestAndSet(
          {mojom::blink::UseCounterFeatureType::kWebFeature,
           static_cast<uint32_t>(web_feature)})) {
    return;
  }

  if (commit_state_ >= kCommited)
    ReportAndTraceMeasurementByFeatureId(web_feature, source_frame);
}

void UseCounterImpl::ReportAndTraceMeasurementByFeatureId(
    WebFeature feature,
    const LocalFrame& source_frame) {
  if (context_ != kDisabledContext) {
    // Note that HTTPArchive tooling looks specifically for this event -
    // see https://github.com/HTTPArchive/httparchive/issues/59
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.feature_usage"),
                 "FeatureFirstUsed", "feature", feature);
    if (context_ != kDefaultContext)
      CountFeature(feature);
    if (LocalFrameClient* client = source_frame.Client())
      client->DidObserveNewFeatureUsage(feature);
    NotifyFeatureCounted(feature);
  }
}

bool UseCounterImpl::IsCounted(WebFeature web_feature) const {
  if (mute_count_)
    return false;

  // PageDestruction is reserved as a scaling factor.
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, web_feature);
  DCHECK_NE(WebFeature::kPageVisits, web_feature);
  DCHECK_GE(WebFeature::kNumberOfFeatures, web_feature);

  return feature_tracker_.Test(
      {mojom::blink::UseCounterFeatureType::kWebFeature,
       static_cast<uint32_t>(web_feature)});
}

void UseCounterImpl::ClearMeasurementForTesting(WebFeature web_feature) {
  feature_tracker_.ResetForTesting(
      {mojom::blink::UseCounterFeatureType::kWebFeature,
       static_cast<uint32_t>(web_feature)});
}

void UseCounterImpl::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
}

void UseCounterImpl::DidCommitLoad(const LocalFrame* frame) {
  const KURL url = frame->GetDocument()->Url();
  if (url.ProtocolIs("chrome-extension"))
    context_ = kExtensionContext;
  if (url.ProtocolIs("file"))
    context_ = kFileContext;

  DCHECK_EQ(kPreCommit, commit_state_);
  commit_state_ = kCommited;
  if (!mute_count_) {
    // If any feature was recorded prior to navigation commits, flush to the
    // browser side.
    for (const UseCounterFeature& feature :
         feature_tracker_.GetRecordedFeatures()) {
      switch (feature.type) {
        case mojom::blink::UseCounterFeatureType::kWebFeature:
          ReportAndTraceMeasurementByFeatureId(
              static_cast<WebFeature>(feature.value), *frame);
          break;
        case mojom::blink::UseCounterFeatureType::kCssProperty:
          ReportAndTraceMeasurementByCSSSampleId(feature.value, frame,
                                                 /* is_animated */ false);
          break;
        case mojom::blink::UseCounterFeatureType::kAnimatedCssProperty:
          ReportAndTraceMeasurementByCSSSampleId(feature.value, frame,
                                                 /* is_animated */ true);
          break;
      }
    }
  }

  // TODO(crbug.com/1196402): move extension histogram to the browser side.
  if (context_ == kExtensionContext || context_ == kFileContext) {
    CountFeature(WebFeature::kPageVisits);
  }
}

bool UseCounterImpl::IsCounted(CSSPropertyID unresolved_property,
                               CSSPropertyType type) const {
  if (unresolved_property == CSSPropertyID::kInvalid) {
    return false;
  }

  return feature_tracker_.Test(
      {ToFeatureType(type),
       static_cast<uint32_t>(GetCSSSampleId(unresolved_property))});
}

void UseCounterImpl::AddObserver(Observer* observer) {
  DCHECK(!observers_.Contains(observer));
  observers_.insert(observer);
}

void UseCounterImpl::ReportAndTraceMeasurementByCSSSampleId(
    int sample_id,
    const LocalFrame* frame,
    bool is_animated) {
  // Note that HTTPArchive tooling looks specifically for this event - see
  // https://github.com/HTTPArchive/httparchive/issues/59
  if (context_ != kDisabledContext && context_ != kExtensionContext) {
    const char* name = is_animated ? "AnimatedCSSFirstUsed" : "CSSFirstUsed";
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.feature_usage"), name,
                 "feature", sample_id);
    if (frame && frame->Client()) {
      frame->Client()->DidObserveNewCssPropertyUsage(
          static_cast<mojom::blink::CSSSampleId>(sample_id), is_animated);
    }
  }
}

void UseCounterImpl::Count(CSSPropertyID property,
                           CSSPropertyType type,
                           const LocalFrame* source_frame) {
  DCHECK(IsCSSPropertyIDWithName(property) ||
         property == CSSPropertyID::kVariable);

  if (mute_count_)
    return;

  uint32_t sample_id = static_cast<uint32_t>(GetCSSSampleId(property));
  if (feature_tracker_.TestAndSet({ToFeatureType(type), sample_id})) {
    return;
  }

  if (commit_state_ >= kCommited) {
    ReportAndTraceMeasurementByCSSSampleId(
        sample_id, source_frame,
        /* is_animated */ type == CSSPropertyType::kAnimation);
  }
}

void UseCounterImpl::Count(WebFeature feature, const LocalFrame* source_frame) {
  if (!source_frame)
    return;
  RecordMeasurement(feature, *source_frame);
}

void UseCounterImpl::NotifyFeatureCounted(WebFeature feature) {
  DCHECK(!mute_count_);
  DCHECK_NE(kDisabledContext, context_);
  HeapHashSet<Member<Observer>> to_be_removed;
  for (auto observer : observers_) {
    if (observer->OnCountFeature(feature))
      to_be_removed.insert(observer);
  }
  observers_.RemoveAll(to_be_removed);
}

void UseCounterImpl::CountFeature(WebFeature feature) const {
  switch (context_) {
    case kDefaultContext:
      // Feature usage for the default context is recorded on the browser side.
      // TODO(dcheng): Where?
      NOTREACHED();
      return;
    case kExtensionContext:
      UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.Extensions.Features", feature,
                                WebFeature::kNumberOfFeatures);
      return;
    case kFileContext:
      UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.File.Features", feature,
                                WebFeature::kNumberOfFeatures);
      return;
    case kDisabledContext:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

}  // namespace blink
