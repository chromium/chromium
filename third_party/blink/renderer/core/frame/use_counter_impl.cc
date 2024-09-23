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
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
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

mojom::blink::UseCounterFeatureType ToFeatureType(
    UseCounterImpl::PermissionsPolicyUsageType type) {
  switch (type) {
    case UseCounterImpl::PermissionsPolicyUsageType::kViolation:
      return mojom::blink::UseCounterFeatureType::
          kPermissionsPolicyViolationEnforce;
    case UseCounterImpl::PermissionsPolicyUsageType::kHeader:
      return mojom::blink::UseCounterFeatureType::kPermissionsPolicyHeader;
    case UseCounterImpl::PermissionsPolicyUsageType::kIframeAttribute:
      return mojom::blink::UseCounterFeatureType::
          kPermissionsPolicyIframeAttribute;
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

bool UseCounterImpl::IsCounted(WebFeature web_feature) const {
  if (mute_count_)
    return false;

  // PageVisits is reserved as a scaling factor.
  DCHECK_NE(web_feature, WebFeature::kPageVisits);
  DCHECK_LE(web_feature, WebFeature::kMaxValue);

  return feature_tracker_.Test(
      {mojom::blink::UseCounterFeatureType::kWebFeature,
       static_cast<uint32_t>(web_feature)});
}

void UseCounterImpl::ClearMeasurementForTesting(WebFeature web_feature) {
  feature_tracker_.ResetForTesting(
      {mojom::blink::UseCounterFeatureType::kWebFeature,
       static_cast<uint32_t>(web_feature)});
}

bool UseCounterImpl::IsWebDXFeatureCounted(WebDXFeature webdx_feature) const {
  if (mute_count_) {
    return false;
  }

  // PageDestruction is reserved as a scaling factor.
  DCHECK_NE(webdx_feature, WebDXFeature::kPageVisits);
  DCHECK_LE(webdx_feature, WebDXFeature::kMaxValue);

  return feature_tracker_.Test(
      {mojom::blink::UseCounterFeatureType::kWebDXFeature,
       static_cast<uint32_t>(webdx_feature)});
}

void UseCounterImpl::ClearMeasurementForTesting(WebDXFeature webdx_feature) {
  feature_tracker_.ResetForTesting(
      {mojom::blink::UseCounterFeatureType::kWebDXFeature,
       static_cast<uint32_t>(webdx_feature)});
}

void UseCounterImpl::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
}

void UseCounterImpl::DidCommitLoad(const LocalFrame* frame) {
  const KURL url = frame->GetDocument()->Url();
  if (CommonSchemeRegistry::IsExtensionScheme(url.Protocol().Ascii())) {
    context_ = kExtensionContext;
  } else if (url.ProtocolIs("file")) {
    context_ = kFileContext;
  } else if (url.ProtocolIsInHTTPFamily()) {
    context_ = kDefaultContext;
  } else {
    // UseCounter is disabled for all other URL schemes.
    context_ = kDisabledContext;
  }

  DCHECK_EQ(kPreCommit, commit_state_);
  commit_state_ = kCommited;

  if (mute_count_)
    return;

  // If any feature was recorded prior to navigation commits, flush to the
  // browser side.
  for (const UseCounterFeature& feature :
       feature_tracker_.GetRecordedFeatures()) {
    if (ReportMeasurement(feature, frame))
      TraceMeasurement(feature);
  }

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

bool UseCounterImpl::IsCounted(const UseCounterFeature& feature) const {
  if (mute_count_)
    return false;

  return feature_tracker_.Test(feature);
}

void UseCounterImpl::AddObserver(Observer* observer) {
  DCHECK(!observers_.Contains(observer));
  observers_.insert(observer);
}

void UseCounterImpl::Count(const UseCounterFeature& feature,
                           const LocalFrame* source_frame) {
  if (!source_frame)
    return;

  if (mute_count_)
    return;

  if (feature_tracker_.TestAndSet(feature)) {
    return;
  }

  if (commit_state_ >= kCommited) {
    if (ReportMeasurement(feature, source_frame))
      TraceMeasurement(feature);
  }
}

void UseCounterImpl::Count(CSSPropertyID property,
                           CSSPropertyType type,
                           const LocalFrame* source_frame) {
  DCHECK(IsCSSPropertyIDWithName(property) ||
         property == CSSPropertyID::kVariable);

  Count({ToFeatureType(type), static_cast<uint32_t>(GetCSSSampleId(property))},
        source_frame);
}

void UseCounterImpl::Count(WebFeature web_feature,
                           const LocalFrame* source_frame) {
  // PageVisits is reserved as a scaling factor.
  DCHECK_NE(web_feature, WebFeature::kPageVisits);
  DCHECK_LE(web_feature, WebFeature::kMaxValue);

  Count({mojom::blink::UseCounterFeatureType::kWebFeature,
         static_cast<uint32_t>(web_feature)},
        source_frame);
}

void UseCounterImpl::CountWebDXFeature(WebDXFeature web_feature,
                                       const LocalFrame* source_frame) {
  // PageVisits is reserved as a scaling factor.
  DCHECK_NE(web_feature, WebDXFeature::kPageVisits);
  DCHECK_LE(web_feature, WebDXFeature::kMaxValue);

  Count({mojom::blink::UseCounterFeatureType::kWebDXFeature,
         static_cast<uint32_t>(web_feature)},
        source_frame);
}

void UseCounterImpl::CountPermissionsPolicyUsage(
    mojom::blink::PermissionsPolicyFeature feature,
    PermissionsPolicyUsageType usage_type,
    const LocalFrame& source_frame) {
  DCHECK_NE(mojom::blink::PermissionsPolicyFeature::kNotFound, feature);

  Count({ToFeatureType(usage_type), static_cast<uint32_t>(feature)},
        &source_frame);
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
      // components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer
      NOTREACHED_IN_MIGRATION();
      return;
    case kExtensionContext:
      UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.Extensions.Features",
                                feature);
      return;
    case kFileContext:
      UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.File.Features", feature);
      return;
    case kDisabledContext:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

bool UseCounterImpl::ReportMeasurement(const UseCounterFeature& feature,
                                       const LocalFrame* frame) {
  if (context_ == kDisabledContext)
    return false;

  if (!frame || !frame->Client())
    return false;
  auto* client = frame->Client();

  if (feature.type() == mojom::blink::UseCounterFeatureType::kWebFeature)
    NotifyFeatureCounted(static_cast<WebFeature>(feature.value()));

  // Report to browser about observed event only when URL is HTTP/HTTPS,
  // as other URL schemes are filtered out in
  // |MetricsWebContentsObserver::DoesTimingUpdateHaveError| anyway.
  if (context_ == kDefaultContext) {
    client->DidObserveNewFeatureUsage(feature);
    return true;
  }

  // WebFeatures in non-default contexts are counted on renderer side.
  if (feature.type() == mojom::blink::UseCounterFeatureType::kWebFeature) {
    CountFeature(static_cast<WebFeature>(feature.value()));
    return true;
  }

  return false;
}

// Note that HTTPArchive tooling looks specifically for this event - see
// https://github.com/HTTPArchive/httparchive/issues/59
void UseCounterImpl::TraceMeasurement(const UseCounterFeature& feature) {
  const char* trace_name = nullptr;
  switch (feature.type()) {
    case mojom::blink::UseCounterFeatureType::kWebFeature:
      trace_name = "FeatureFirstUsed";
      break;
    case mojom::blink::UseCounterFeatureType::kWebDXFeature:
      trace_name = "WebDXFeatureFirstUsed";
      break;
    case mojom::blink::UseCounterFeatureType::kAnimatedCssProperty:
      trace_name = "AnimatedCSSFirstUsed";
      break;
    case mojom::blink::UseCounterFeatureType::kCssProperty:
      trace_name = "CSSFirstUsed";
      break;
    case mojom::blink::UseCounterFeatureType::
        kPermissionsPolicyViolationEnforce:
    case mojom::blink::UseCounterFeatureType::kPermissionsPolicyHeader:
    case mojom::blink::UseCounterFeatureType::kPermissionsPolicyIframeAttribute:
      // TODO(crbug.com/1206004): Add trace event for permissions policy metrics
      // gathering.
      return;
  }
  DCHECK(trace_name);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.feature_usage"), trace_name,
               "feature", feature.value());
}
}  // namespace blink
