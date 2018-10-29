// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_thumbnail_generator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace activity_services {

ThumbnailGeneratorBlock ThumbnailGeneratorForTab(Tab* tab) {
  DCHECK(tab);
  DCHECK(tab.webState);
  // Do not generate thumbnails for incognito tabs.
  if (tab.webState->GetBrowserState()->IsOffTheRecord()) {
    return ^UIImage*(CGSize const& size) { return nil; };
  } else {
    __weak Tab* weakTab = tab;
    return ^UIImage*(CGSize const& size) {
      Tab* strongTab = weakTab;
      if (!strongTab || !strongTab.webState)
        return nil;

      UIImage* snapshot = SnapshotTabHelper::FromWebState(strongTab.webState)
                              ->GenerateSnapshot(/*with_overlays=*/false,
                                                 /*visible_frame_only=*/true);

      if (!snapshot)
        return nil;

      return ResizeImage(snapshot, size, ProjectionMode::kAspectFillAlignTop,
                         /*opaque=*/YES);
    };
  }
}

}  // namespace activity_services
