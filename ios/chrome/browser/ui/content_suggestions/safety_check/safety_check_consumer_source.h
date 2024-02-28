// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_CONSUMER_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_CONSUMER_SOURCE_H_

@protocol SafetyCheckMagicStackConsumer;

// The source of any consumer of MagicStack events.
@protocol SafetyCheckConsumerSource

// Consumer for this model.
- (void)addConsumer:(id<SafetyCheckMagicStackConsumer>)consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_CONSUMER_SOURCE_H_
