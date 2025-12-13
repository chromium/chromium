// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_UI_TAB_RESUMPTION_CONSUMER_SOURCE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_UI_TAB_RESUMPTION_CONSUMER_SOURCE_H_

@protocol TabResumptionConsumer;

// Protocol for an object that provides data updates to consumers for the Tab
// Resumption module.
@protocol TabResumptionConsumerSource

// Adds a `consumer` to receive updates on Tab Resumption data.
- (void)addConsumer:(id<TabResumptionConsumer>)consumer;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_UI_TAB_RESUMPTION_CONSUMER_SOURCE_H_
