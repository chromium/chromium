// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_AUDIENCE_H_

enum class SafetyCheckItemType;
@class SetUpListItemView;

// Interface to handle Safety Check card user events.
@protocol SafetyCheckAudience

// Called when a Safety Check item is selected by the user.
- (void)didSelectSafetyCheckItem:(SafetyCheckItemType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_AUDIENCE_H_
