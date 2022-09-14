// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LINK_TO_TEXT_LINK_TO_TEXT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_LINK_TO_TEXT_LINK_TO_TEXT_CONSUMER_H_

@class LinkToTextPayload;

// Protocol for communicating link-to-text updates.
@protocol LinkToTextConsumer

// Invoked when a link-to-text was successfully generated, with `payload`
// containing all the information around that deep-link.
- (void)generatedPayload:(LinkToTextPayload*)payload;

// Invoked when the link-to-text generation failed.
- (void)linkGenerationFailed;

@end

#endif  // IOS_CHROME_BROWSER_UI_LINK_TO_TEXT_LINK_TO_TEXT_CONSUMER_H_