// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/public/features.h"

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
