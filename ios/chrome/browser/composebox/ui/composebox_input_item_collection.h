// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_COLLECTION_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_COLLECTION_H_

#import <UIKit/UIKit.h>

namespace base {
class UnguessableToken;
}

@class ComposeboxInputItem;
@class ComposeboxInputItemCollection;

// Delegate for a `ComposeboxInputItemCollection`.
@protocol ComposeboxInputItemCollectionDelegate <NSObject>

// Informs the caller that the collection was updated.
- (void)composeboxInputItemCollectionDidUpdateItems:
    (ComposeboxInputItemCollection*)composeboxInputItemCollection;

@end

// A ordered container for `ComposeboxInputItem`, modeling the attachments
// carousel in the inputplate.
@interface ComposeboxInputItemCollection : NSObject

// Creates a new input item collection with the given attachment limit.
- (instancetype)initWithAttachmentLimit:(size_t)attachmentLimit;

// The delegate for this instance.
@property(nonatomic, weak) id<ComposeboxInputItemCollectionDelegate> delegate;

// The contained items in this collection.
@property(nonatomic, readonly, copy)
    NSArray<ComposeboxInputItem*>* containedItems;

// The number of objects in the array.
@property(nonatomic, readonly) size_t count;

// Whether the collection is empty.
@property(nonatomic, readonly, getter=isEmpty) BOOL empty;

// Whether the collection has at least one image added.
@property(nonatomic, readonly) BOOL hasImage;

// Whether the collection contains a tab or a file attachment.
@property(nonatomic, readonly) BOOL hasTabOrFile;

// Whether more attachment can be added.
@property(nonatomic, readonly) BOOL canAddMoreAttachments;

// The available slots in this collection.
@property(nonatomic, readonly) size_t availableSlots;

// The number of non tab attachments.
@property(nonatomic, readonly) size_t nonTabAttachmentCount;

// The first item in this collection, or `nil` if not present.
@property(nonatomic, readonly) ComposeboxInputItem* firstItem;

// Adds an items to the collection.
- (void)addItem:(ComposeboxInputItem*)item;

// Replaces the items in the collection with the given updated items.
- (void)replaceWithItems:(NSArray<ComposeboxInputItem*>*)updatedItems;

// Removes an item from the collection.
- (void)removeItem:(ComposeboxInputItem*)item;

// Removes all items from the collection.
- (void)clearItems;

// Returns the item with the given `identifier` or nil if not found.
- (ComposeboxInputItem*)itemForIdentifier:(base::UnguessableToken)identifier;

// Returns the item with the given `serverToken` or nil if not found.
- (ComposeboxInputItem*)itemForServerToken:(base::UnguessableToken)serverToken;

// Whether the given asset identified by the `assetID` was loaded.
- (BOOL)assetAlreadyLoaded:(NSString*)assetID;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_COLLECTION_H_
