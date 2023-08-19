// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_metrics_helper.h"

#import <string>

#import "base/metrics/histogram_functions.h"
#import "components/omnibox/browser/page_classification_functions.h"

namespace {

std::string HistogramPageClassSuffix(
    metrics::OmniboxEventProto::PageClassification page_classification) {
  std::string suffix = "Other";
  if (omnibox::IsNTPPage(page_classification)) {
    suffix = "NTP";
  } else if (omnibox::IsSearchResultsPage(page_classification)) {
    suffix = "SRP";
  }  // use default value of Web pages
  return "." + suffix;
}

}  // namespace

void RecordSuggestionsListScrolled(
    metrics::OmniboxEventProto::PageClassification page_classification,
    bool was_scrolled) {
  base::UmaHistogramBoolean("IOS.Omnibox.SuggestionsListScrolled" +
                                HistogramPageClassSuffix(page_classification),
                            was_scrolled);
}
