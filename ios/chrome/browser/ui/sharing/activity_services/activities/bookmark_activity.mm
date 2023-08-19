// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/bookmark_activity.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

NSString* const kBookmarkActivityType = @"com.google.chrome.bookmarkActivity";

}  // namespace

@interface BookmarkActivity ()
// Whether or not the page is bookmarked.
@property(nonatomic, assign) BOOL bookmarked;
// The bookmark model used to validate if a page was bookmarked.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// The URL of the page to be bookmarked.
@property(nonatomic, assign) GURL URL;
// The title of the page to be bookmarked.
@property(nonatomic, copy) NSString* title;
// The handler invoked when the activity is performed.
@property(nonatomic, weak) id<BookmarksCommands> handler;
// User's preferences service.
@property(nonatomic, assign) PrefService* prefService;
@end

@implementation BookmarkActivity

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                    handler:(id<BookmarksCommands>)handler
                prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _URL = URL;
    _title = title;
    _bookmarkModel = bookmarkModel;
    _handler = handler;
    _prefService = prefService;

    _bookmarked = _bookmarkModel && _bookmarkModel->loaded() &&
                  _bookmarkModel->IsBookmarked(_URL);
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kBookmarkActivityType;
}

- (NSString*)activityTitle {
  if (self.bookmarked) {
    return l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK);
  }
  return l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS);
}

- (UIImage*)activityImage {
  NSString* symbolName =
      self.bookmarked ? kEditActionSymbol : kAddBookmarkActionSymbol;
  return DefaultSymbolWithPointSize(symbolName, kSymbolActionPointSize);
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  // Don't show the add/remove bookmark activity if we have an invalid
  // bookmarkModel, or if editing bookmarks is disabled in the prefs.
  return self.bookmarkModel && [self isEditBookmarksEnabledInPrefs];
}

- (void)prepareWithActivityItems:(NSArray*)activityItems {
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  // Activity must be marked finished first, otherwise it may dismiss UI
  // presented by the bookmark command below.
  [self activityDidFinish:YES];

  [self.handler createOrEditBookmarkWithURL:[[URLWithTitle alloc]
                                                initWithURL:self.URL
                                                      title:self.title]];
}

#pragma mark - Private

// Verifies if, based on preferences, the user can edit their bookmarks or not.
- (BOOL)isEditBookmarksEnabledInPrefs {
  return self.prefService->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled);
}

@end
