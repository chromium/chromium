// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_CONSUMER_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_CONSUMER_SOURCE_H_

@protocol TipsMagicStackConsumer;

// The source of any consumer of Tips module events.
@protocol TipsModuleConsumerSource

// Consumer for this model.
- (void)addConsumer:(id<TipsMagicStackConsumer>)consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_CONSUMER_SOURCE_H_
