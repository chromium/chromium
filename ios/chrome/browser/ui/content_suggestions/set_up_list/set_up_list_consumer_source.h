// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONSUMER_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONSUMER_SOURCE_H_

@protocol SetUpListConsumer;

// The source of any consumer of Set Up List events.
@protocol SetUpListConsumerSource

// Consumer for this model.
- (void)addConsumer:(id<SetUpListConsumer>)consumer;
- (void)removeConsumer:(id<SetUpListConsumer>)consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONSUMER_SOURCE_H_
