// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace blink {

CSSVariableData* DocumentStyleEnvironmentVariables::ResolveVariable(
    const AtomicString& name,
    WTF::Vector<unsigned> indices,
    bool record_metrics) {
  if (record_metrics) {
    RecordVariableUsage(name);
  }

  // Mark the variable as seen so we will invalidate the style if we change it.
  seen_variables_.insert(name);
  return StyleEnvironmentVariables::ResolveVariable(name, std::move(indices));
}

const FeatureContext* DocumentStyleEnvironmentVariables::GetFeatureContext()
    const {
  return document_->GetExecutionContext();
}

CSSVariableData* DocumentStyleEnvironmentVariables::ResolveVariable(
    const AtomicString& name,
    WTF::Vector<unsigned> indices) {
  return ResolveVariable(name, std::move(indices), true /* record_metrics */);
}

void DocumentStyleEnvironmentVariables::InvalidateVariable(
    const AtomicString& name) {
  DCHECK(document_);

  // Invalidate the document if we have seen this variable on this document.
  if (seen_variables_.Contains(name)) {
    document_->GetStyleEngine().EnvironmentVariableChanged();
  }

  StyleEnvironmentVariables::InvalidateVariable(name);
}

DocumentStyleEnvironmentVariables::DocumentStyleEnvironmentVariables(
    StyleEnvironmentVariables& parent,
    Document& document)
    : StyleEnvironmentVariables(parent), document_(&document) {
  if (RuntimeEnabledFeatures::CSSPreferredTextScaleEnabled()) {
    SetPreferredTextScale(
        document_->GetSettings()->GetAccessibilityFontScaleFactor());
  }
}

void DocumentStyleEnvironmentVariables::RecordVariableUsage(
    const AtomicString& name) {
  UseCounter::Count(document_, WebFeature::kCSSEnvironmentVariable);

  if (name == "safe-area-inset-top") {
    UseCounter::Count(document_,
                      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetTop);
  } else if (name == "safe-area-inset-left") {
    UseCounter::Count(document_,
                      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetLeft);
  } else if (name == "safe-area-inset-bottom") {
    UseCounter::Count(document_,
                      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom);
    // Record usage for viewport-fit histogram.
    // TODO(https://crbug.com/1482559) remove after data captured (end of
    // 2023).
    if (document_->GetFrame()->IsOutermostMainFrame()) {
      UseCounter::Count(document_,
                        WebFeature::kViewportFitCoverOrSafeAreaInsetBottom);
    }
  } else if (name == "safe-area-inset-right") {
    UseCounter::Count(document_,
                      WebFeature::kCSSEnvironmentVariable_SafeAreaInsetRight);
  } else if (name == "safe-area-max-inset-bottom") {
    UseCounter::Count(
        document_, WebFeature::kCSSEnvironmentVariable_SafeAreaMaxInsetBottom);
  } else if (name == GetVariableName(UADefinedVariable::kPreferredTextScale,
                                     /* feature_context */ nullptr)) {
    UseCounter::Count(document_,
                      WebFeature::kCSSEnvironmentVariable_PreferredTextScale);
  } else {
    // Do nothing if this is an unknown variable.
  }
}
namespace {

double SnapToClosestFontScaleBucket(double raw_font_scale) {
  static const std::array<double, 7> font_scale_buckets = {0.85, 1,   1.15, 1.3,
                                                           1.5,  1.8, 2};

  // Handle cases where the input value is outside the range of literals.
  if (raw_font_scale <= font_scale_buckets.front()) {
    return font_scale_buckets.front();
  }
  if (raw_font_scale >= font_scale_buckets.back()) {
    return font_scale_buckets.back();
  }

  // std::lower_bound finds the first element >= raw_font_scale.
  const auto it = std::lower_bound(font_scale_buckets.begin(),
                                   font_scale_buckets.end(), raw_font_scale);

  const double higher_candidate = *it;
  // The element before 'it' is the other candidate.
  // Safe because we handled edge cases above.
  const double lower_candidate = *(it - 1);

  const double diff_to_lower = std::abs(raw_font_scale - lower_candidate);
  const double diff_to_higher = std::abs(raw_font_scale - higher_candidate);

  return (diff_to_lower <= diff_to_higher) ? lower_candidate : higher_candidate;
}

}  // namespace

void DocumentStyleEnvironmentVariables::SetPreferredTextScale(
    double raw_font_scale_from_os) {
  SetVariable(
      UADefinedVariable::kPreferredTextScale,
      String::Number(SnapToClosestFontScaleBucket(raw_font_scale_from_os)));
}

}  // namespace blink
