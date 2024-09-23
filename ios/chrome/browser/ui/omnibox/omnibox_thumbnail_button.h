// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THUMBNAIL_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THUMBNAIL_BUTTON_H_

#import <UIKit/UIKit.h>

// Thumbnail button optionally shown in the Omnibox.
//
// When this button is in `UIControlStateSelected` state, it displays a blue
// overlay indicating the image is about to be dismissed.
@interface OmniboxThumbnailButton : UIButton

// The thumbnail image shown in the omnibox or `nil` if there is no image set.
@property(nonatomic, strong) UIImage* thumbnailImage;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THUMBNAIL_BUTTON_H_
