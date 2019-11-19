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

#include "third_party/blink/renderer/core/frame/use_counter_helper.h"

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
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

namespace blink {

UseCounterMuteScope::UseCounterMuteScope(const Element& element)
    : loader_(element.GetDocument().Loader()) {
  if (loader_)
    loader_->GetUseCounterHelper().MuteForInspector();
}

UseCounterMuteScope::~UseCounterMuteScope() {
  if (loader_)
    loader_->GetUseCounterHelper().UnmuteForInspector();
}

UseCounterHelper::UseCounterHelper(Context context, CommitState commit_state)
    : mute_count_(0), context_(context), commit_state_(commit_state) {}

void UseCounterHelper::MuteForInspector() {
  mute_count_++;
}

void UseCounterHelper::UnmuteForInspector() {
  mute_count_--;
}

void UseCounterHelper::RecordMeasurement(WebFeature feature,
                                         const LocalFrame& source_frame) {
  if (mute_count_)
    return;

  // PageDestruction is reserved as a scaling factor.
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, feature);
  DCHECK_NE(WebFeature::kPageVisits, feature);
  DCHECK_GE(WebFeature::kNumberOfFeatures, feature);

  int feature_id = static_cast<int>(feature);
  if (features_recorded_[feature_id])
    return;
  if (commit_state_ >= kCommited)
    ReportAndTraceMeasurementByFeatureId(feature_id, source_frame);

  features_recorded_.set(feature_id);
}

void UseCounterHelper::ReportAndTraceMeasurementByFeatureId(
    int feature_id,
    const LocalFrame& source_frame) {
  if (context_ != kDisabledContext) {
    // Note that HTTPArchive tooling looks specifically for this event -
    // see https://github.com/HTTPArchive/httparchive/issues/59
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.feature_usage"),
                 "FeatureFirstUsed", "feature", feature_id);
    if (context_ != kDefaultContext)
      FeaturesHistogram().Count(feature_id);
    if (LocalFrameClient* client = source_frame.Client())
      client->DidObserveNewFeatureUsage(static_cast<WebFeature>(feature_id));
    NotifyFeatureCounted(static_cast<WebFeature>(feature_id));
  }
}

bool UseCounterHelper::HasRecordedMeasurement(WebFeature feature) const {
  if (mute_count_)
    return false;

  // PageDestruction is reserved as a scaling factor.
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, feature);
  DCHECK_NE(WebFeature::kPageVisits, feature);
  DCHECK_GE(WebFeature::kNumberOfFeatures, feature);

  return features_recorded_[static_cast<size_t>(feature)];
}

void UseCounterHelper::ClearMeasurementForTesting(WebFeature feature) {
  features_recorded_.reset(static_cast<size_t>(feature));
}

void UseCounterHelper::Trace(blink::Visitor* visitor) {
  visitor->Trace(observers_);
}

void UseCounterHelper::DidCommitLoad(const LocalFrame* frame) {
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
    for (wtf_size_t feature_id = 0; feature_id < features_recorded_.size();
         ++feature_id) {
      if (features_recorded_[feature_id])
        ReportAndTraceMeasurementByFeatureId(feature_id, *frame);
    }
    for (wtf_size_t sample_id = 0; sample_id < css_recorded_.size();
         ++sample_id) {
      if (css_recorded_[sample_id])
        ReportAndTraceMeasurementByCSSSampleId(sample_id, frame, false);
      if (animated_css_recorded_[sample_id])
        ReportAndTraceMeasurementByCSSSampleId(sample_id, frame, true);
    }

    // TODO(loonybear): move extension histogram to the browser side.
    if (context_ == kExtensionContext || context_ == kFileContext) {
      FeaturesHistogram().Count(static_cast<int>(WebFeature::kPageVisits));
    }
  }
}

bool UseCounterHelper::IsCounted(CSSPropertyID unresolved_property,
                                 CSSPropertyType type) const {
  if (unresolved_property == CSSPropertyID::kInvalid) {
    return false;
  }
  int sample_id = static_cast<int>(GetCSSSampleId(unresolved_property));
  switch (type) {
    case CSSPropertyType::kDefault:
      return css_recorded_[sample_id];
    case CSSPropertyType::kAnimation:
      return animated_css_recorded_[sample_id];
  }
}

void UseCounterHelper::AddObserver(Observer* observer) {
  DCHECK(!observers_.Contains(observer));
  observers_.insert(observer);
}

void UseCounterHelper::ReportAndTraceMeasurementByCSSSampleId(
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
          static_cast<mojom::CSSSampleId>(sample_id), is_animated);
    }
  }
}

void UseCounterHelper::Count(CSSPropertyID property,
                             CSSPropertyType type,
                             const LocalFrame* source_frame) {
  DCHECK(isCSSPropertyIDWithName(property) ||
         property == CSSPropertyID::kVariable);

  if (mute_count_)
    return;

  int sample_id = static_cast<int>(GetCSSSampleId(property));
  switch (type) {
    case CSSPropertyType::kDefault:
      if (css_recorded_[sample_id])
        return;
      if (commit_state_ >= kCommited)
        ReportAndTraceMeasurementByCSSSampleId(sample_id, source_frame, false);

      css_recorded_.set(sample_id);
      break;
    case CSSPropertyType::kAnimation:
      if (animated_css_recorded_[sample_id])
        return;
      if (commit_state_ >= kCommited)
        ReportAndTraceMeasurementByCSSSampleId(sample_id, source_frame, true);

      animated_css_recorded_.set(sample_id);
      break;
  }
}

void UseCounterHelper::Count(WebFeature feature,
                             const LocalFrame* source_frame) {
  if (!source_frame)
    return;
  RecordMeasurement(feature, *source_frame);
}

void UseCounterHelper::NotifyFeatureCounted(WebFeature feature) {
  DCHECK(!mute_count_);
  DCHECK_NE(kDisabledContext, context_);
  HeapHashSet<Member<Observer>> to_be_removed;
  for (auto observer : observers_) {
    if (observer->OnCountFeature(feature))
      to_be_removed.insert(observer);
  }
  observers_.RemoveAll(to_be_removed);
}

EnumerationHistogram& UseCounterHelper::FeaturesHistogram() const {
  DEFINE_STATIC_LOCAL(blink::EnumerationHistogram, extension_histogram,
                      ("Blink.UseCounter.Extensions.Features",
                       static_cast<int32_t>(WebFeature::kNumberOfFeatures)));
  DEFINE_STATIC_LOCAL(blink::EnumerationHistogram, file_histogram,
                      ("Blink.UseCounter.File.Features",
                       static_cast<int32_t>(WebFeature::kNumberOfFeatures)));
  // Track what features/properties have been reported to the browser side
  // histogram.
  switch (context_) {
    case kDefaultContext:
      // The default features histogram is being recorded on the browser side.
      NOTREACHED();
      break;
    case kExtensionContext:
      return extension_histogram;
    case kFileContext:
      return file_histogram;
    case kDisabledContext:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  blink::EnumerationHistogram* null = nullptr;
  return *null;
}

}  // namespace blink
