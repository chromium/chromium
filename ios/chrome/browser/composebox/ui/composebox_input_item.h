// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_H_

#import <UIKit/UIKit.h>

namespace base {
class UnguessableToken;
}

// Enum for the loading state of an item.
enum class ComposeboxInputItemState {
  kLoading,
  kUploading,
  kLoaded,
  kError,
};

// Enum for the aim input item type.
enum class ComposeboxInputItemType {
  kComposeboxInputItemTypeImage,
  kComposeboxInputItemTypeFile,
  kComposeboxInputItemTypeTab,
};

// Data object for an item in the AIM input.
@interface ComposeboxInputItem : NSObject <NSCopying>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithComposeboxInputItemType:(ComposeboxInputItemType)type
                                        assetID:(NSString*)assetID
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithComposeboxInputItemType:(ComposeboxInputItemType)type;

// The item's identifier.
@property(nonatomic, assign, readonly) const base::UnguessableToken& identifier;
// The server token for this item, used to identify the item on the server.
@property(nonatomic, assign) base::UnguessableToken serverToken;
// The preview image for this item.
@property(nonatomic, strong) UIImage* previewImage;
// The icon image for this item. Only set for kComposeboxInputItemTypeFile and
// kComposeboxInputItemTypeTab types.
@property(nonatomic, strong) UIImage* leadingIconImage;
// The title for this item.
@property(nonatomic, copy) NSString* title;
// The current state of the item.
@property(nonatomic, assign) ComposeboxInputItemState state;
// The type of the input item.
@property(nonatomic, assign) ComposeboxInputItemType type;
// Optional, uniquely identifying the asset the item is associated with.
@property(nonatomic, copy, readonly) NSString* assetID;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_H_
