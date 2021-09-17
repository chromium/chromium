// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_LINK_NO_PREVIEW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_LINK_NO_PREVIEW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class FaviconAttributes;

// View Controller showing the information for a link when a preview of the
// destination is not displayed.
@interface LinkNoPreviewViewController : UIViewController

// Initializes with the |title| and |subtitle| to be displayed.
- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Configures the Favicon with |attributes|.
- (void)configureFaviconWithAttributes:(FaviconAttributes*)attributes;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_LINK_NO_PREVIEW_VIEW_CONTROLLER_H_
