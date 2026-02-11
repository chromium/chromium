// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_PUBLIC_TAB_RESUMPTION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_PUBLIC_TAB_RESUMPTION_CONSTANTS_H_

#import <UIKit/UIKit.h>

#import "base/feature_list.h"

// Kill switch to disable remote tab resumption.
BASE_DECLARE_FEATURE(kIOSRemoteTabResumptionKillSwitch);

// Accessibility identifier for the TabResumptionView.
extern NSString* const kTabResumptionViewIdentifier;

// Command line flag to show the item immediately without waiting for favicon.
// Mainly used in tests to avoid network requests.
extern const char kTabResumptionShowItemImmediately[];

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_PUBLIC_TAB_RESUMPTION_CONSTANTS_H_
