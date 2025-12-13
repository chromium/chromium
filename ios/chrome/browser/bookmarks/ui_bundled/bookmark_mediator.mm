// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_mediator.h"

#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkNode;

@implementation BookmarkMediator {
  // Bookmark model for this mediator.
  base::WeakPtr<bookmarks::BookmarkModel> _bookmarkModel;

  // Prefs model for this mediator.
  raw_ptr<PrefService> _prefs;

  // Authentication service for this mediator.
  base::WeakPtr<AuthenticationService> _authenticationService;

  // Sync service for this mediator.
  raw_ptr<syncer::SyncService> _syncService;
}

+ (void)registerProfilePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(
      prefs::kIosBookmarkLastUsedFolderReceivingBookmarks,
      kLastUsedBookmarkFolderNone);
  registry->RegisterIntegerPref(
      prefs::kIosBookmarkLastUsedStorageReceivingBookmarks,
      static_cast<int>(BookmarkStorageType::kLocalOrSyncable));
}

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                                prefs:(PrefService*)prefs
                authenticationService:
                    (AuthenticationService*)authenticationService
                          syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    _bookmarkModel = bookmarkModel->AsWeakPtr();
    _prefs = prefs;
    _authenticationService = authenticationService->GetWeakPtr();
    _syncService = syncService;
  }
  return self;
}

- (void)disconnect {
  _bookmarkModel = nullptr;
  _prefs = nullptr;
  _authenticationService = nullptr;
  _syncService = nullptr;
}

- (SnackbarMessage*)addBookmarkWithTitle:(NSString*)title
                                     URL:(const GURL&)URL
                              editAction:(void (^)())editAction {
  RecordModuleFreshnessSignal(ContentSuggestionsModuleType::kShortcuts, _prefs);
  base::RecordAction(base::UserMetricsAction("BookmarkAdded"));

  const BookmarkNode* defaultFolder =
      GetDefaultBookmarkFolder(_prefs, _bookmarkModel.get());
  _bookmarkModel->AddNewURL(defaultFolder, defaultFolder->children().size(),
                            base::SysNSStringToUTF16(title), URL);

  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.handler = editAction;
  action.title =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_SNACKBAR_EDIT_BOOKMARK);

  NSString* text = bookmark_utils_ios::messageForAddingBookmarksInFolder(
      defaultFolder, _bookmarkModel.get(), !IsLastUsedBookmarkFolderSet(_prefs),
      /*showCount=*/false, /*count=*/1, _authenticationService, _syncService);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:text];
  message.action = action;
  return message;
}

- (SnackbarMessage*)bulkAddBookmarksWithURLs:(NSArray<NSURL*>*)URLs
                                  viewAction:(void (^)())viewAction {
  CHECK([URLs count] > 0, base::NotFatalUntil::M152);
  base::RecordAction(base::UserMetricsAction("IOSBookmarksAddedInBulk"));

  const BookmarkNode* defaultFolder =
      GetDefaultBookmarkFolder(_prefs, _bookmarkModel.get());

  // Add bookmarks and keep track of successful additions.
  int successfullyAddedBookmarks = 0;
  for (NSURL* NSURL in URLs) {
    GURL URL = net::GURLWithNSURL(NSURL);

    if (!URL.is_valid()) {
      continue;
    }

    // Construct the title from domain + path (stripping trailing slash from
    // path).
    std::string path = URL.GetWithoutRef().GetWithoutFilename().GetPath();
    if (path.length() > 0) {
      path.pop_back();
    }
    NSString* title = base::SysUTF8ToNSString(URL.GetHost() + path);

    const BookmarkNode* existingBookmark =
        _bookmarkModel->GetMostRecentlyAddedUserNodeForURL(URL);

    if (!existingBookmark) {
      _bookmarkModel->AddNewURL(defaultFolder, defaultFolder->children().size(),
                                base::SysNSStringToUTF16(title), URL);
      successfullyAddedBookmarks++;
    }
  }

  base::UmaHistogramCounts100("IOS.Bookmarks.BulkAddURLsCount",
                              successfullyAddedBookmarks);

  // Create snackbar message.
  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.handler = viewAction;
  action.title =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_SNACKBAR_VIEW_BOOKMARKS);

  NSString* result = bookmark_utils_ios::messageForAddingBookmarksInFolder(
      defaultFolder, _bookmarkModel.get(), !IsLastUsedBookmarkFolderSet(_prefs),
      /*showCount=*/true, successfullyAddedBookmarks, _authenticationService,
      _syncService);

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:result];
  message.action = action;

  return message;
}

- (SnackbarMessage*)addBookmarks:(NSArray<URLWithTitle*>*)URLs
                        toFolder:(const BookmarkNode*)folder {
  for (URLWithTitle* urlWithTitle in URLs) {
    RecordModuleFreshnessSignal(ContentSuggestionsModuleType::kShortcuts,
                                _prefs);
    base::RecordAction(base::UserMetricsAction("BookmarkAdded"));
    _bookmarkModel->AddNewURL(folder, folder->children().size(),
                              base::SysNSStringToUTF16(urlWithTitle.title),
                              urlWithTitle.URL);
  }

  NSString* text = bookmark_utils_ios::messageForAddingBookmarksInFolder(
      folder, _bookmarkModel.get(), /*choosenByUser=*/YES,
      /*showCount=*/false, URLs.count, _authenticationService, _syncService);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:text];
  return message;
}

@end
