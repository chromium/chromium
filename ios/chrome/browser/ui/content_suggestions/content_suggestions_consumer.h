// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_

// Consumer protocol for the content suggestions view controller.
@protocol ContentSuggestionsConsumer
// Notifies the consumer to set the content suggestions enabled
// based on the user setting. Setting the feed to disabled removes the section
// entirely, including the feed header.
- (void)setContentSuggestionsEnabled:(BOOL)enabled;
// Notifies the consumer to set the content suggestions visibility
// based on the user setting. Setting the feed to invisible hides the feed
// content, but retains the feed header.
- (void)setContentSuggestionsVisible:(BOOL)visible;
@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
