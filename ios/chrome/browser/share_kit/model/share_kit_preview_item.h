// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_PREVIEW_ITEM_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_PREVIEW_ITEM_H_

#import <UIKit/UIKit.h>

// A preview item for the Join flow. Corresponds to a tab from the shared tab
// group.
@interface ShareKitPreviewItem : NSObject

// The image of the preview item. Typically a favicon.
@property(nonatomic, strong) UIImage* image;

// The title of the preview item. Typically the domain of the shared link.
@property(nonatomic, copy) NSString* title;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_PREVIEW_ITEM_H_
