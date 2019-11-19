// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_H_

#import <UIKit/UIKit.h>

class GURL;

typedef NS_ENUM(NSInteger, OmniboxIconType) {
  OmniboxIconTypeSuggestionIcon,
  OmniboxIconTypeImage,
  OmniboxIconTypeFavicon
};

// This protocol represents all the parts necessary to display a composited
// omnibox icon. Most icons have a background and a main image. If the main
// image is an icon (not an image from the web), it will be tinted some color.
// |OmniboxIconView| is the preferred way to consume this protocol and display
// the icons.
@protocol OmniboxIcon <NSObject>

@property(nonatomic, assign, readonly) OmniboxIconType iconType;
@property(nonatomic, assign, readonly) GURL imageURL;
@property(nonatomic, strong, readonly) UIImage* iconImage;
@property(nonatomic, strong, readonly) UIColor* iconImageTintColor;
@property(nonatomic, strong, readonly) UIImage* backgroundImage;
@property(nonatomic, strong, readonly) UIColor* backgroundImageTintColor;
@property(nonatomic, strong, readonly) UIImage* overlayImage;
@property(nonatomic, strong, readonly) UIColor* overlayImageTintColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_H_
