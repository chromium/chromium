// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_CONSUMER_H_
#define IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_CONSUMER_H_

@protocol LinkPreviewConsumer <NSObject>

// Updates the consumer with the current loading state.
- (void)setLoadingState:(BOOL)loading;

// Updates the consumer with the current progress of the WebState.
- (void)setLoadingProgressFraction:(double)progress;

// Updates the consumer with the current origin.
- (void)setPreviewOrigin:(NSString*)origin;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_CONSUMER_H_
