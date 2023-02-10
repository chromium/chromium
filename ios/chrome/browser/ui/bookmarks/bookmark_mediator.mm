// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/url_with_title.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {
const int64_t kLastUsedFolderNone = -1;
}  // namespace

@interface BookmarkMediator () {
  // Bookmark model for this mediator.
  bookmarks::BookmarkModel* _bookmarkModel;

  // Prefs model for this mediator.
  PrefService* _prefs;
}
@end

@implementation BookmarkMediator

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosBookmarkFolderDefault,
                              kLastUsedFolderNone);
}

- (instancetype)initWithWithBookmarkModel:
                    (bookmarks::BookmarkModel*)bookmarkModel
                                    prefs:(PrefService*)prefs {
  self = [super init];
  if (self) {
    _bookmarkModel = bookmarkModel;
    _prefs = prefs;
  }
  return self;
}

- (MDCSnackbarMessage*)addBookmarkWithTitle:(NSString*)title
                                        URL:(const GURL&)URL
                                 editAction:(void (^)())editAction {
  base::RecordAction(base::UserMetricsAction("BookmarkAdded"));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  const BookmarkNode* defaultFolder = [self folderForNewBookmarks];
  _bookmarkModel->AddNewURL(defaultFolder, defaultFolder->children().size(),
                            base::SysNSStringToUTF16(title), URL);

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = editAction;
  action.title = l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON);
  action.accessibilityIdentifier = @"Edit";

  NSString* folderTitle =
      bookmark_utils_ios::TitleForBookmarkNode(defaultFolder);
  BOOL usesDefaultFolder =
      (_prefs->GetInt64(prefs::kIosBookmarkFolderDefault) ==
       kLastUsedFolderNone);
  NSString* text = [self messageForAddingBookmarksInFolder:!usesDefaultFolder
                                                     title:folderTitle
                                                     count:1];
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.action = action;
  message.category = bookmark_utils_ios::kBookmarksSnackbarCategory;
  return message;
}

- (MDCSnackbarMessage*)addBookmarks:(NSArray<URLWithTitle*>*)URLs
                           toFolder:(const BookmarkNode*)folder {
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  for (URLWithTitle* urlWithTitle in URLs) {
    base::RecordAction(base::UserMetricsAction("BookmarkAdded"));
    _bookmarkModel->AddNewURL(folder, folder->children().size(),
                              base::SysNSStringToUTF16(urlWithTitle.title),
                              urlWithTitle.URL);
  }

  NSString* folderTitle = bookmark_utils_ios::TitleForBookmarkNode(folder);
  NSString* text = [self messageForAddingBookmarksInFolder:(folderTitle.length)
                                                     title:folderTitle
                                                     count:URLs.count];
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.category = bookmark_utils_ios::kBookmarksSnackbarCategory;
  return message;
}

#pragma mark - Private

- (const BookmarkNode*)folderForNewBookmarks {
  const BookmarkNode* defaultFolder = _bookmarkModel->mobile_node();
  int64_t node_id = _prefs->GetInt64(prefs::kIosBookmarkFolderDefault);
  if (node_id == kLastUsedFolderNone) {
    node_id = defaultFolder->id();
  }
  const BookmarkNode* result =
      bookmarks::GetBookmarkNodeByID(_bookmarkModel, node_id);

  if (result) {
    return result;
  }

  return defaultFolder;
}

// The localized strings for adding bookmarks.
// `addFolder`: whether the folder name should appear in the message
// `folderTitle`: The name of the folder. Assumed to be non-nil if `addFolder`
// is true. `count`: the number of bookmarks. Used for localization.
- (NSString*)messageForAddingBookmarksInFolder:(BOOL)addFolder
                                         title:(NSString*)folderTitle
                                         count:(int)count {
  std::u16string result;
  if (addFolder) {
    std::u16string pattern =
        l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER);
    result = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "title",
        base::SysNSStringToUTF16(folderTitle));
  } else {
    result =
        l10n_util::GetPluralStringFUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED, count);
  }
  return base::SysUTF16ToNSString(result);
}

@end
