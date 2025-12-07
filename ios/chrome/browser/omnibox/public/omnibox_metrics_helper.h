// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_METRICS_HELPER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_METRICS_HELPER_H_

#import <UIKit/UIKit.h>

#import "third_party/metrics_proto/omnibox_event.pb.h"

// Enum representing the type copy in the omnibox. Current values should not be
// renumbered.
// LINT.IfChange(OmniboxCopyType)
enum class OmniboxCopyType {
  kText = 0,
  kEditedURL = 1,
  kPreEditURL = 2,
  kMaxValue = kPreEditURL,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSOmniboxCopyType)

// Records whether the suggestion list (popup) was scrolled during this omnibox
// interaction.
void RecordSuggestionsListScrolled(
    metrics::OmniboxEventProto::PageClassification page_classification,
    bool was_scrolled);

// Records a copy event in the omnibox.
void RecordOmniboxCopy(OmniboxCopyType type);

#endif  // IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_METRICS_HELPER_H_
