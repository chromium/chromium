// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_DRAG_DROP_METRICS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_DRAG_DROP_METRICS_H_

#import <Foundation/Foundation.h>

// LINT.IfChange

// Key of UMA DragDropEvent histograms.
extern NSString* const kUmaTabStripViewDragDropTabsEvent;
extern NSString* const kUmaTabStripViewDragDropGroupsEvent;
extern const char kUmaGridViewDragDropTabsEvent[];
extern const char kUmaGridViewDragDropGroupsEvent[];
extern const char kUmaGridViewDragDropMultiSelectEvent[];
extern const char kUmaPinnedViewDragDropTabsEvent[];

// Key of UMA DragOrigin histograms.
extern const char kUmaTabStripViewDragOrigin[];
extern const char kUmaTabStripViewGroupDragOrigin[];
extern const char kUmaGridViewDragOrigin[];
extern const char kUmaGridViewGroupDragOrigin[];
extern const char kUmaPinnedViewDragOrigin[];
extern const char kUmaGroupViewDragOrigin[];

#ifdef __cplusplus

#pragma mark - C++ Declarations

// Values of UMA DragDrop histograms. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class DragDropItem {
  // An item is dragged.
  kDragBegin = 0,
  // An item is dropped at the same index position.
  kDragEndAtSameIndex = 1,
  // An item is dropped at a new index position.
  kDragEndAtNewIndex = 2,
  // An item is dropped outside of its collection view.
  kDragEndInOtherCollection = 3,
  kMaxValue = kDragEndInOtherCollection
};

// Values of UMA DragOrigin histograms. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class DragItemOrigin {
  kSameCollection = 0,
  kSameBrowser = 1,
  kOtherBrowser = 2,
  kOther = 3,
  kMaxValue = kOther
};

#else

#pragma mark - Swift Declarations

// Swift implementation of `DragItemOrigin`.
typedef NS_ENUM(NSInteger, DragDropTabs) {
  // A tab is dragged.
  DragDropTabsDragBegin = 0,
  // A tab is dropped at the same index position.
  DragDropTabsDragEndAtSameIndex = 1,
  // A tab is dropped at a new index position.
  DragDropTabsDragEndAtNewIndex = 2,
  // A tab is dropped outside of its collection view.
  DragDropTabsDragEndInOtherCollection = 3,
  DragDropTabsMaxValue = DragDropTabsDragEndInOtherCollection
};

#endif
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/histograms.xml)

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_DRAG_DROP_METRICS_H_
