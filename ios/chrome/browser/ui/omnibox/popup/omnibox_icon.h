// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_H_

#import <UIKit/UIKit.h>

@class CrURL;

typedef NS_ENUM(NSInteger, OmniboxIconType) {
  OmniboxIconTypeSuggestionIcon,
  OmniboxIconTypeImage,
  OmniboxIconTypeFavicon
};

/// This protocol represents all the parts necessary to display a composited
/// omnibox icon. Most icons have a background and a main image. If the main
/// image is an icon (not an image from the web), it will be tinted some color.
/// `OmniboxIconView` is the preferred way to consume this protocol and display
/// the icons.
@protocol OmniboxIcon <NSObject>

@property(nonatomic, assign, readonly) OmniboxIconType iconType;
@property(nonatomic, strong, readonly) CrURL* imageURL;
@property(nonatomic, strong, readonly) UIImage* iconImage;
@property(nonatomic, strong, readonly) UIColor* iconImageTintColor;
@property(nonatomic, strong, readonly) UIColor* backgroundImageTintColor;
@property(nonatomic, strong, readonly) UIColor* borderColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_H_
