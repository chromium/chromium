// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_DRAG_DROP_METRICS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_DRAG_DROP_METRICS_H_

// Key of UMA DragDrop histograms.
extern const char kUmaGridViewDragDropTabs[];
extern const char kUmaPinnedViewDragDropTabs[];

// Values of UMA DragDrop histograms. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class DragDropTabs {
  // A tab is dragged.
  kDragBegin = 0,
  // A tab is dropped at the same index position.
  kDragEndAtSameIndex = 1,
  // A tab is dropped at a new index position.
  kDragEndAtNewIndex = 2,
  kMaxValue = kDragEndAtNewIndex
};

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_COLLECTION_DRAG_DROP_METRICS_H_
