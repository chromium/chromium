// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_consumer.h"

// ViewController for the link preview. It displays a loaded webState UIView.
@interface LinkPreviewViewController : UIViewController <LinkPreviewConsumer>

// Inits the view controller with the `webStateView` and the `origin` of the
// preview.
- (instancetype)initWithView:(UIView*)webStateView
                      origin:(NSString*)origin NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNAme
                         bundle:(NSBundle*)nibBundle NS_UNAVAILABLE;

// Resets the auto layout for preview.
- (void)resetAutoLayoutForPreview;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_VIEW_CONTROLLER_H_
