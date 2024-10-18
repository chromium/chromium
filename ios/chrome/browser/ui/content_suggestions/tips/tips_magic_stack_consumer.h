// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MAGIC_STACK_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MAGIC_STACK_CONSUMER_H_

@class TipsModuleState;

// Interface for listening to events occurring in `TipsMagicStackMediator`.
@protocol TipsMagicStackConsumer

// Indicates that the latest Tips state has changed.
- (void)tipsStateDidChange:(TipsModuleState*)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MAGIC_STACK_CONSUMER_H_
