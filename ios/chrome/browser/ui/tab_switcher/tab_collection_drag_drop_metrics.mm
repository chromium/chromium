// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

extern const char kUmaGridViewDragDropTabs[] = "IOS.TabSwitcher.DragDropTabs";
extern const char kUmaPinnedViewDragDropTabs[] =
    "IOS.TabSwitcher.PinnedTabs.DragDropTabs";

extern const char kUmaGridViewDragOrigin[] = "IOS.TabSwitcher.DragOrigin";
extern const char kUmaPinnedViewDragOrigin[] =
    "IOS.TabSwitcher.PinnedTabs.DragOrigin";
