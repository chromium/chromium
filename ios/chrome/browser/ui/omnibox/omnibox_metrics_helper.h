// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_METRICS_HELPER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_METRICS_HELPER_H_

#import <UIKit/UIKit.h>

#import "third_party/metrics_proto/omnibox_event.pb.h"

// Records whether the suggestion list (popup) was scrolled during this omnibox
// interaction.
void RecordSuggestionsListScrolled(
    metrics::OmniboxEventProto::PageClassification page_classification,
    bool was_scrolled);

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_METRICS_HELPER_H_
