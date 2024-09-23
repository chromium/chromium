// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"

NSString* const kUmaTabStripViewDragDropTabsEvent =
    @"IOS.TabStrip.DragDropTabs";
NSString* const kUmaTabStripViewDragDropGroupsEvent =
    @"IOS.TabStrip.DragDropGroups";
extern const char kUmaGridViewDragDropTabsEvent[] =
    "IOS.TabSwitcher.DragDropTabs";
extern const char kUmaGridViewDragDropGroupsEvent[] =
    "IOS.TabSwitcher.DragDropGroups";
extern const char kUmaGridViewDragDropMultiSelectEvent[] =
    "IOS.TabSwitcher.DragDropMultiSelect";
extern const char kUmaPinnedViewDragDropTabsEvent[] =
    "IOS.TabSwitcher.PinnedTabs.DragDropTabs";

extern const char kUmaTabStripViewDragOrigin[] = "IOS.TabStrip.DragOrigin";
extern const char kUmaTabStripViewGroupDragOrigin[] =
    "IOS.TabStrip.DragOrigin.Group";
extern const char kUmaGridViewDragOrigin[] = "IOS.TabSwitcher.DragOrigin";
extern const char kUmaGridViewGroupDragOrigin[] =
    "IOS.TabSwitcher.DragOrigin.Group";
extern const char kUmaPinnedViewDragOrigin[] =
    "IOS.TabSwitcher.PinnedTabs.DragOrigin";
extern const char kUmaGroupViewDragOrigin[] =
    "IOS.TabSwitcher.Group.DragOrigin";
