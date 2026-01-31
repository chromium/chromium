// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"

#include "third_party/blink/renderer/core/css/font_size_functions.h"
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
    Vector<unsigned> indices,
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
    Vector<unsigned> indices) {
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
  UpdatePreferredTextScaleFromDocument();
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

void DocumentStyleEnvironmentVariables::UpdatePreferredTextScaleFromDocument() {
  double scale_factor;

  // For compat, we don't expose env(preferred-text-scale)'s true value to pages
  // in WebView if the page has no meta text-scale tag and the app does not
  // enable autosizing.
  //
  // WebView defaults to inflating ALL text on the page, so if there's a page
  // that uses env(preferred-text-scale) to inflate *parts* of the page, those
  // parts will get double-scaled (once along with everything else, then once
  // again by env()).

  if (document_->TextScaleMetaTagPresent()) {
    // But if a page includes meta, they are signaling to us that the page will
    // handle scaling themselves, so we populate env() to let them use it.
    // Elsewhere, in response to meta, we have disabled Webview inflating all
    // text.
    scale_factor =
        FontSizeFunctions::SnapToClosestFontScaleBucket(
            document_->GetSettings()->GetAccessibilityFontScaleFactor()) *
        (document_->GetSettings()->GetDefaultFontSize() / 16.0);
  } else {
    const bool should_hide_env_to_prevent_double_scaling =
        document_->GetSettings()->GetScaleAllFontsIfNoMetaTextScaleTag() &&
        !document_->GetSettings()->GetTextAutosizingEnabled();

    scale_factor =
        should_hide_env_to_prevent_double_scaling
            ? 1.0
            : FontSizeFunctions::SnapToClosestFontScaleBucket(
                  document_->GetSettings()->GetAccessibilityFontScaleFactor());
  }

  SetVariable(UADefinedVariable::kPreferredTextScale,
              String::Number(scale_factor));
}

}  // namespace blink
