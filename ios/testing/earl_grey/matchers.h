// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_MATCHERS_H_
#define IOS_TESTING_EARL_GREY_MATCHERS_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;

namespace testing {

// Matcher for element with accessibility label corresponding to `label` and
// accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label);

// Matcher for a UI element to tap to dismiss an alert (e.g. context menu),
// where `cancel_text` is the localized text used for the action sheet cancel
// control.
// On phones, where the alert is an action sheet, this will be a matcher for the
// menu item with `cancel_text` as its label.
// On tablets, where the alert is a popover, this will be a matcher for some
// element outside of the popover.
id<GREYMatcher> ElementToDismissAlert(NSString* cancel_text);

// Matcher for an element whose accessibility label contains `substring`.
id<GREYMatcher> ElementWithAccessibilityLabelSubstring(NSString* substring);

// Matcher for the back button of a navigation bar.
id<GREYMatcher> NavigationBarBackButton();

}  // namespace testing

#endif  // IOS_TESTING_EARL_GREY_MATCHERS_H_
