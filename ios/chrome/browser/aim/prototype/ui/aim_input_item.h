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

// Data object for an item in the AIM input.
@interface AIMInputItem : NSObject <NSCopying>

// The file token for this item, which also serves as its unique identifier.
@property(nonatomic, assign, readonly) const base::UnguessableToken& fileToken;
// The preview image for this item.
@property(nonatomic, strong) UIImage* previewImage;
// The current state of the item.
@property(nonatomic, assign) AIMInputItemState state;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_H_
