// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_IMAGE_PREVIEW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_IMAGE_PREVIEW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View Controller showing a preview of an image.
@interface ImagePreviewViewController : UIViewController

// Initializes with |preferredContentSize|.
- (instancetype)initWithPreferredContentSize:(CGSize)preferredContentSize
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Sets the displayed image to |image|.
- (void)updateImage:(UIImage*)image;

// Sets the displayed image to |data|.
- (void)updateImageData:(NSData*)data;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_IMAGE_PREVIEW_VIEW_CONTROLLER_H_
