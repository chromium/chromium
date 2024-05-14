// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of the symbol image used in content suggestions.
const CGFloat kSymbolContentSuggestionsPointSize = 22;

}  // namespace

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
    case NTPCollectionShortcutTypeWhatsNew:
      return l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_WHATS_NEW);
    case NTPCollectionShortcutTypeCount:
      NOTREACHED_IN_MIGRATION();
      return @"";
  }
}

UIImage* SymbolForCollectionShortcutType(NTPCollectionShortcutType type) {
  switch (type) {
    case NTPCollectionShortcutTypeBookmark:
      return DefaultSymbolTemplateWithPointSize(
          kBookmarksSymbol, kSymbolContentSuggestionsPointSize);
    case NTPCollectionShortcutTypeReadingList:
      return CustomSymbolTemplateWithPointSize(
          kReadingListSymbol, kSymbolContentSuggestionsPointSize);
    case NTPCollectionShortcutTypeRecentTabs:
      return CustomSymbolTemplateWithPointSize(
          kRecentTabsSymbol, kSymbolContentSuggestionsPointSize);
    case NTPCollectionShortcutTypeHistory:
      return DefaultSymbolTemplateWithPointSize(
          kHistorySymbol, kSymbolContentSuggestionsPointSize);
    case NTPCollectionShortcutTypeWhatsNew:
      return DefaultSymbolTemplateWithPointSize(
          kCheckmarkSealSymbol, kSymbolContentSuggestionsPointSize);
    case NTPCollectionShortcutTypeCount:
      NOTREACHED_IN_MIGRATION();
      return nil;
  }
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
