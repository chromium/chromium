// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_CONSUMER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the ContextualPanelEntrypointViewController.
@protocol ContextualPanelEntrypointConsumer

// Update the consumer with the image to be shown on the entrypoint badge.
- (void)setEntrypointImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_CONSUMER_H_
