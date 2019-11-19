// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_VIEW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_VIEW_H_

#import <UIKit/UIKit.h>

@protocol OmniboxIcon;
@protocol FaviconRetriever;
@protocol ImageRetriever;

// This class is used to display |OmniboxIcon|s. It handles the multiple image
// views neceesary to get the correct compositing behavior.
@interface OmniboxIconView : UIView

// Used to fetch favicons.
@property(nonatomic, weak) id<FaviconRetriever> faviconRetriever;
// Used to fetch other images (rich entities, answers, etc.)
@property(nonatomic, weak) id<ImageRetriever> imageRetriever;
// Used for testing to check whether this view is displaying anything.
@property(nonatomic, readonly) UIImage* mainImage;

- (void)prepareForReuse;

- (void)setOmniboxIcon:(id<OmniboxIcon>)omniboxIcon;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_VIEW_H_
