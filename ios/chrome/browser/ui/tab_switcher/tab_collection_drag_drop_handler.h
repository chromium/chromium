// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_DRAG_DROP_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_DRAG_DROP_HANDLER_H_

#import <UIKit/UIKit.h>

@class TabGroupItem;
@class TabSwitcherItem;

// A protocol for objects that handle drag and drop interactions for a
// collection view involving the model layer.
@protocol TabCollectionDragDropHandler

// Returns a value which represents how a drag activity should be resolved when
// the user drops a drag item. `session` contains pertinent information
// including the drag item.
- (UIDropOperation)dropOperationForDropSession:(id<UIDropSession>)session
                                       toIndex:(NSUInteger)destinationIndex;

// Tells the receiver to incorporate the `dragItem` into the model layer at the
// `destinationIndex`. `fromSameCollection` is an indication that the operation
// is a reorder within the same collection. `dragItem` must have a localObject,
// which means the item is dragged from within the same app.
- (void)dropItem:(UIDragItem*)dragItem
               toIndex:(NSUInteger)destinationIndex
    fromSameCollection:(BOOL)fromSameCollection;

// Tells the receiver to asynchronously extract data from `itemProvider` into
// the model layer at the `destinationIndex`. `placeholderContext` is used to
// delete the placeholder once the item is ready to be inserted into the model
// layer.
- (void)dropItemFromProvider:(NSItemProvider*)itemProvider
                     toIndex:(NSUInteger)destinationIndex
          placeholderContext:
              (id<UICollectionViewDropPlaceholderContext>)placeholderContext;

@optional

// Returns a drag item encapsulating all necessary information to perform
// valid drop operations for the given `item`.
// Note that this drag item may be dropped anywhere,
// including within the same collection, another view, or other apps.
- (UIDragItem*)dragItemForItem:(TabSwitcherItem*)item;

// Returns a drag item encapsulating all necessary information to perform
// valid drop operations for the given `tabGroupItem`.
// Note that this drag item cannot be dropped within other apps.
- (UIDragItem*)dragItemForTabGroupItem:(TabGroupItem*)tabGroupItem;

// Tells the receiver that the drag session will begin for the
// `tabSwitcherItem`.
- (void)dragWillBeginForTabSwitcherItem:(TabSwitcherItem*)tabSwitcherItem;

// Tells the receiver that the drag session will begin for the `tabGroupItem`.
- (void)dragWillBeginForTabGroupItem:(TabGroupItem*)tabGroupItem;

// Tells the receiver that the drag session did end.
- (void)dragSessionDidEnd;

// Returns the drag items list of selected element in selection mode. Selection
// mode is only supported for incognito and regular grids.
- (NSArray<UIDragItem*>*)allSelectedDragItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_DRAG_DROP_HANDLER_H_
