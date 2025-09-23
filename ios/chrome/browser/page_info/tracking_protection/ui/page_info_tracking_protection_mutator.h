// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_UI_PAGE_INFO_TRACKING_PROTECTION_MUTATOR_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_UI_PAGE_INFO_TRACKING_PROTECTION_MUTATOR_H_

// Defines the APIs for the mediator to handle requests from the ViewController.
@protocol PageInfoTrackingProtectionMutator <NSObject>

// Toggles tracking protection state on the current site.
- (void)toggleTrackingProtections;

@end

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_UI_PAGE_INFO_TRACKING_PROTECTION_MUTATOR_H_
