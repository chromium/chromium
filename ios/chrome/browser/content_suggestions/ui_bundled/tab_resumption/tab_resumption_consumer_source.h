// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_TAB_RESUMPTION_CONSUMER_SOURCE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_TAB_RESUMPTION_CONSUMER_SOURCE_H_

@protocol TabResumptionConsumer;

@protocol TabResumptionConsumerSource
- (void)addConsumer:(id<TabResumptionConsumer>)consumer;
@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_TAB_RESUMPTION_CONSUMER_SOURCE_H_
