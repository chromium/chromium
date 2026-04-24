// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ITEM_H_

#import <UIKit/UIKit.h>

enum class ComposeboxMenuItemType {
  kUnknown = 0,
  kAIM,
  kCreateImage,
  kDeepSearch,
  kCanvas,
  kModelRegular,
  kModelAuto,
  kModelThinking,
  kAttachmentTabs,
  kAttachmentCamera,
  kAttachmentGallery,
  kAttachmentFiles,
};

// Represents a menu item in the Composebox menu.
@interface ComposeboxMenuItem : NSObject

// The composebox menu item title.
@property(nonatomic, copy, readonly) NSString* title;
// The composebox menu item image.
@property(nonatomic, strong, readonly) UIImage* image;
// The composebox menu item type.
@property(nonatomic, assign, readonly) ComposeboxMenuItemType type;

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ITEM_H_
