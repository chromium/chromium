// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_capability_element_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

// Capability element histograms always start with
// Blink.CapabilityElement.{Geolocation|UserMedia|Camera|Microphone}
#define CAPABILITY_ELEMENT_BASE_HISTOGRAM_PATTERN "Blink.CapabilityElement.%s"

constexpr char kCapabilityElementBaseHistogramPattern[] =
    CAPABILITY_ELEMENT_BASE_HISTOGRAM_PATTERN;
constexpr char kUserInteractionHistogramPattern[] =
    CAPABILITY_ELEMENT_BASE_HISTOGRAM_PATTERN ".UserInteraction";

const char* GetCapabilityNameForHistogram(const QualifiedName& tag_name) {
  if (tag_name == html_names::kGeolocationTag) {
    return "Geolocation";
  } else if (tag_name == html_names::kInstallTag) {
    return "Install";
  } else if (tag_name == html_names::kUsermediaTag) {
    return "UserMedia";
  } else {
    NOTREACHED();
  }
}
}  // namespace

void RecordPermissionElementUseCounter(Document& document,
                                       const QualifiedName& tag_name) {
  if (tag_name == html_names::kGeolocationTag) {
    UseCounter::Count(document, WebFeature::kHTMLGeolocationElement);
  } else if (tag_name == html_names::kInstallTag) {
    UseCounter::Count(document, WebFeature::kHTMLInstallElement);
  } else if (tag_name == html_names::kUsermediaTag) {
    UseCounter::Count(document, WebFeature::kHTMLUserMediaElement);
  } else {
    NOTREACHED();
  }
}

void RecordPermissionElementUserInteractionAccepted(
    const QualifiedName& tag_name,
    bool accepted) {
  base::UmaHistogramBoolean(
      base::StringPrintf(kUserInteractionHistogramPattern,
                         GetCapabilityNameForHistogram(tag_name)) +
          ".Accepted",
      accepted);
}

void RecordPermissionElementUserInteractionDeniedReason(
    const QualifiedName& tag_name,
    UserInteractionDeniedReason reason) {
  base::UmaHistogramEnumeration(
      base::StringPrintf(kUserInteractionHistogramPattern,
                         GetCapabilityNameForHistogram(tag_name)) +
          ".Denied.Reason",
      reason);
}

void RecordPermissionElementInvalidStyleReason(const QualifiedName& tag_name,
                                               InvalidStyleReason reason) {
  base::UmaHistogramEnumeration(
      base::StringPrintf(kCapabilityElementBaseHistogramPattern,
                         GetCapabilityNameForHistogram(tag_name)) +
          ".InvalidStyleReason",
      reason);
}

}  // namespace blink
