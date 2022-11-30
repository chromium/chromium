// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

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

@interface BookmarkMediator ()

// BrowserState for this mediator.
@property(nonatomic, assign) ChromeBrowserState* browserState;

@end

@implementation BookmarkMediator

@synthesize browserState = _browserState;

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosBookmarkFolderDefault,
                              kLastUsedFolderNone);
}

+ (const BookmarkNode*)folderForNewBookmarksInBrowserState:
    (ChromeBrowserState*)browserState {
  bookmarks::BookmarkModel* bookmarks =
      ios::BookmarkModelFactory::GetForBrowserState(browserState);
  const BookmarkNode* defaultFolder = bookmarks->mobile_node();

  PrefService* prefs = browserState->GetPrefs();
  int64_t node_id = prefs->GetInt64(prefs::kIosBookmarkFolderDefault);
  if (node_id == kLastUsedFolderNone)
    node_id = defaultFolder->id();
  const BookmarkNode* result =
      bookmarks::GetBookmarkNodeByID(bookmarks, node_id);

  if (result)
    return result;

  return defaultFolder;
}

+ (void)setFolderForNewBookmarks:(const BookmarkNode*)folder
                  inBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(folder && folder->is_folder());
  browserState->GetPrefs()->SetInt64(prefs::kIosBookmarkFolderDefault,
                                     folder->id());
}

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
  }
  return self;
}

- (MDCSnackbarMessage*)addBookmarkWithTitle:(NSString*)title
                                        URL:(const GURL&)URL
                                 editAction:(void (^)())editAction {
  base::RecordAction(base::UserMetricsAction("BookmarkAdded"));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  const BookmarkNode* defaultFolder =
      [[self class] folderForNewBookmarksInBrowserState:self.browserState];
  BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(self.browserState);
  bookmarkModel->AddNewURL(defaultFolder, defaultFolder->children().size(),
                           base::SysNSStringToUTF16(title), URL);

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = editAction;
  action.title = l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON);
  action.accessibilityIdentifier = @"Edit";

  NSString* folderTitle =
      bookmark_utils_ios::TitleForBookmarkNode(defaultFolder);
  NSString* text =
      self.browserState->GetPrefs()->GetInt64(
          prefs::kIosBookmarkFolderDefault) != kLastUsedFolderNone
          ? l10n_util::GetNSStringF(IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER,
                                    base::SysNSStringToUTF16(folderTitle))
          : l10n_util::GetNSString(IDS_IOS_BOOKMARK_PAGE_SAVED);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.action = action;
  message.category = bookmark_utils_ios::kBookmarksSnackbarCategory;
  return message;
}

- (MDCSnackbarMessage*)addBookmarks:(NSArray<URLWithTitle*>*)URLs
                           toFolder:(const BookmarkNode*)folder {
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(self.browserState);

  for (URLWithTitle* urlWithTitle in URLs) {
    base::RecordAction(base::UserMetricsAction("BookmarkAdded"));
    bookmarkModel->AddNewURL(folder, folder->children().size(),
                             base::SysNSStringToUTF16(urlWithTitle.title),
                             urlWithTitle.URL);
  }

  NSString* folderTitle = bookmark_utils_ios::TitleForBookmarkNode(folder);
  NSString* text =
      folderTitle && [folderTitle length]
          ? l10n_util::GetNSStringF(IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER,
                                    base::SysNSStringToUTF16(folderTitle))
          : l10n_util::GetNSString(IDS_IOS_BOOKMARK_PAGE_SAVED);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.category = bookmark_utils_ios::kBookmarksSnackbarCategory;
  return message;
}

@end
