// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_IMAGE_PREVIEW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_IMAGE_PREVIEW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View Controller showing a preview of an image.
@interface ImagePreviewViewController : UIViewController

// Sets the displayed image to |data|.
- (void)updateImageData:(NSData*)data;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_IMAGE_PREVIEW_VIEW_CONTROLLER_H_
