// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_LINK_NO_PREVIEW_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_LINK_NO_PREVIEW_VIEW_H_

#import <UIKit/UIKit.h>

@class FaviconAttributes;

// View showing the information for a link when a preview of the destination is
// not displayed.
@interface LinkNoPreviewView : UIView

// Initializes the view with its |title| and |subtitle|.
- (instancetype)initWithTitle:(NSString*)title subtitle:(NSString*)subtitle;

// Sets the favicon for the preview.
- (void)configureWithAttributes:(FaviconAttributes*)attributes;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_LINK_NO_PREVIEW_VIEW_H_
