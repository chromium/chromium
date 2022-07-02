// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/public/features.h"

#include "base/metrics/field_trial_params.h"
#include "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kPopupMenuBookmarkStringParamName[] =
    "PopupMenuBookmarkStringParamName";
const char kPopupMenuBookmarkStringParamAddABookmark[] =
    "PopupMenuBookmarkStringParamAddABookmark";
const char kPopupMenuBookmarkStringParamAddToBookmarks[] =
    "PopupMenuBookmarkStringParamAddToBookmarks";
const char kPopupMenuBookmarkStringParamBookmarkThisPage[] =
    "PopupMenuBookmarkStringParamBookmarkThisPage";

const base::Feature kBookmarkString{"BookmarkString",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

int GetBookmarkStringID() {
  std::string label_variant = base::GetFieldTrialParamValueByFeature(
      kBookmarkString, kPopupMenuBookmarkStringParamName);
  if (label_variant == kPopupMenuBookmarkStringParamAddABookmark) {
    return IDS_IOS_TOOLS_MENU_ADD_A_BOOKMARK;
  } else if (label_variant == kPopupMenuBookmarkStringParamAddToBookmarks) {
    return IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS;
  } else if (label_variant == kPopupMenuBookmarkStringParamBookmarkThisPage) {
    return IDS_IOS_TOOLS_MENU_BOOKMARK_THIS_PAGE;
  }
  return IDS_IOS_TOOLS_MENU_BOOKMARK;
}
