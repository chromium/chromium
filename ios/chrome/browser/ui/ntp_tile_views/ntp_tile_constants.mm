// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_constants.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Returns the title to use for a cell with |action|.
NSString* TitleForCollectionShortcutType(NTPCollectionShortcutType type) {
  switch (type) {
    case NTPCollectionShortcutTypeBookmark:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS);
    case NTPCollectionShortcutTypeReadingList:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST);
    case NTPCollectionShortcutTypeRecentTabs:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS);
    case NTPCollectionShortcutTypeHistory:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_HISTORY);
    case NTPCollectionShortcutTypeCount:
      NOTREACHED();
      return @"";
      break;
  }
}

// Returns the image to use for a cell with |action|.
UIImage* ImageForCollectionShortcutType(NTPCollectionShortcutType type) {
  NSString* imageName = nil;
  switch (type) {
    case NTPCollectionShortcutTypeBookmark:
      imageName = @"ntp_bookmarks_icon";
      break;
    case NTPCollectionShortcutTypeReadingList:
      imageName = @"ntp_readinglist_icon";
      break;
    case NTPCollectionShortcutTypeRecentTabs:
      imageName = @"ntp_recent_icon";
      break;
    case NTPCollectionShortcutTypeHistory:
      imageName = @"ntp_history_icon";
      break;
    case NTPCollectionShortcutTypeCount:
      NOTREACHED();
      break;
  }
  return [[UIImage imageNamed:imageName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

NSString* AccessibilityLabelForReadingListCellWithCount(int count) {
  BOOL hasMultipleArticles = count > 1;
  int messageID =
      hasMultipleArticles
          ? IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST_ACCESSIBILITY_LABEL
          : IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST_ACCESSIBILITY_LABEL_ONE_UNREAD;
  if (hasMultipleArticles) {
    return l10n_util::GetNSStringF(
        messageID, base::SysNSStringToUTF16([@(count) stringValue]));
  } else {
    return l10n_util::GetNSString(messageID);
  }
}
