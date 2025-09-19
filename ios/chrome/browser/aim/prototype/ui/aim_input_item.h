// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_H_

#import <UIKit/UIKit.h>

namespace base {
class UnguessableToken;
}

// Enum for the loading state of an item.
enum class AIMInputItemState {
  kLoading,
  kUploading,
  kLoaded,
  kError,
};

// Enum for the aim input item type.
enum class AIMInputItemType {
  kAIMInputItemTypeImage,
  kAIMInputItemTypeFile,
  kAIMInputItemTypeTab,
};

// Data object for an item in the AIM input.
@interface AIMInputItem : NSObject <NSCopying>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithAimInputItemType:(AIMInputItemType)type
    NS_DESIGNATED_INITIALIZER;

// The file token for this item, which also serves as its unique identifier.
@property(nonatomic, assign, readonly) const base::UnguessableToken& token;
// The preview image for this item.
@property(nonatomic, strong) UIImage* previewImage;
// The icon image for this item. Only set for kAIMInputItemTypeFile and
// kAIMInputItemTypeTab types.
@property(nonatomic, strong) UIImage* leadingIconImage;
// The title for this item.
@property(nonatomic, copy) NSString* title;
// The current state of the item.
@property(nonatomic, assign) AIMInputItemState state;
// The type of the input item.
@property(nonatomic, assign) AIMInputItemType type;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_H_
