// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_IMAGE_PREVIEW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_IMAGE_PREVIEW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace web {
class WebState;
}

// A UI view controller that loads an image in a WKWebView to be used in a
// context menu preview.
@interface ImagePreviewViewController : UIViewController

- (instancetype)initWithSrcURL:(NSURL*)URL
                      webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

// Starts the loading of `URL` so it can be displayed in the preview.
- (void)loadPreview;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNAme
                         bundle:(NSBundle*)nibBundle NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_IMAGE_PREVIEW_VIEW_CONTROLLER_H_
