// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_MAGIC_STACK_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_MAGIC_STACK_CONSUMER_H_

@class SafetyCheckState;

// Interface for listening to events occurring in SafetyCheckMagicStackMediator.
@protocol SafetyCheckMagicStackConsumer

// Indicates that the latest SafetyCheck state has changed.
- (void)safetyCheckStateDidChange:(SafetyCheckState*)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_MAGIC_STACK_CONSUMER_H_
