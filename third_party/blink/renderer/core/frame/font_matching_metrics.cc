// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/font_matching_metrics.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/dactyloscoper.h"

namespace blink {

FontMatchingMetrics::FontMatchingMetrics(ExecutionContext* execution_context)
    : execution_context_(execution_context) {}

void FontMatchingMetrics::ReportSuccessfulFontFamilyMatch(
    const AtomicString& font_family_name) {
  if (font_family_name.IsNull()) {
    return;
  }
  ReportLocalFontExistenceByUniqueOrFamilyName(font_family_name,
                                               /*font_exists=*/true);
}

void FontMatchingMetrics::ReportFailedFontFamilyMatch(
    const AtomicString& font_family_name) {
  if (font_family_name.IsNull()) {
    return;
  }
  ReportLocalFontExistenceByUniqueOrFamilyName(font_family_name,
                                               /*font_exists=*/false);
}

void FontMatchingMetrics::ReportSuccessfulLocalFontMatch(
    const AtomicString& font_name) {
  if (font_name.IsNull()) {
    return;
  }
  ReportLocalFontExistenceByUniqueOrFamilyName(font_name, /*font_exists=*/true);
}

void FontMatchingMetrics::ReportFailedLocalFontMatch(
    const AtomicString& font_name) {
  if (font_name.IsNull()) {
    return;
  }
  ReportLocalFontExistenceByUniqueOrFamilyName(font_name,
                                               /*font_exists=*/false);
}

void FontMatchingMetrics::ReportLocalFontExistenceByUniqueOrFamilyName(
    const AtomicString& font_name,
    bool font_exists) {
  if (font_name.IsNull()) {
    return;
  }
  Dactyloscoper::TraceFontLookup(
      execution_context_, font_name,
      Dactyloscoper::FontLookupType::kUniqueOrFamilyName);
}

}  // namespace blink
