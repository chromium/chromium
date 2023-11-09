// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_app_interface.h"

#import <vector>

#import "base/format_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/titled_url_match.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/bookmarks/common/storage_type.h"
#import "components/prefs/pref_service.h"
#import "components/query_parser/query_parser.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/nserror_util.h"
#import "ui/base/models/tree_node_iterator.h"
#import "url/gurl.h"

@implementation BookmarkEarlGreyAppInterface

#pragma mark - Public Interface

+ (NSError*)clearBookmarks {
  bookmarks::BookmarkModel* localOrSyncableBookmarkModel =
      [BookmarkEarlGreyAppInterface localOrSyncableBookmarkModel];
  bookmarks::BookmarkModel* accountBookmarkModel =
      [BookmarkEarlGreyAppInterface accountBookmarkModel];
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browserState->GetPrefs()];
  BOOL removeSucceeded = RemoveAllUserBookmarksIOS(browserState);
  if (!removeSucceeded) {
    return testing::NSErrorWithLocalizedDescription(
        @"Failed to remove some user boomkark");
  }
  // Checking whether managed bookmarks remain, in which case return false.
  if (localOrSyncableBookmarkModel->HasBookmarks()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Local/Syncable bookmark model is not empty. Probably has managed "
        @"bookmark.");
  }
  if (accountBookmarkModel && accountBookmarkModel->HasBookmarks()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Account bookmark model is not empty. Probably has managed "
        @"bookmarks.");
  }
  return nil;
}

+ (void)clearBookmarksPositionCache {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

+ (NSError*)setupStandardBookmarksUsingFirstURL:(NSString*)firstURL
                                      secondURL:(NSString*)secondURL
                                       thirdURL:(NSString*)thirdURL
                                      fourthURL:(NSString*)fourthURL
                                      inStorage:
                                          (bookmarks::StorageType)storageType {
  NSError* bookmarkModelsLoadedError =
      [BookmarkEarlGreyAppInterface waitForBookmarkModelsLoaded];
  if (bookmarkModelsLoadedError) {
    return bookmarkModelsLoadedError;
  }
  bookmarks::BookmarkModel* bookmarkModel =
      [BookmarkEarlGreyAppInterface bookmarkModelOfStorage:storageType];

  NSString* firstTitle = @"First URL";
  const GURL firstGURL = GURL(base::SysNSStringToUTF8(firstURL));
  bookmarkModel->AddURL(bookmarkModel->mobile_node(), 0,
                        base::SysNSStringToUTF16(firstTitle), firstGURL);

  NSString* secondTitle = @"Second URL";
  const GURL secondGURL = GURL(base::SysNSStringToUTF8(secondURL));
  bookmarkModel->AddURL(bookmarkModel->mobile_node(), 0,
                        base::SysNSStringToUTF16(secondTitle), secondGURL);

  NSString* frenchTitle = @"French URL";
  const GURL thirdGURL = GURL(base::SysNSStringToUTF8(thirdURL));
  bookmarkModel->AddURL(bookmarkModel->mobile_node(), 0,
                        base::SysNSStringToUTF16(frenchTitle), thirdGURL);

  NSString* folderTitle = @"Folder 1";
  const bookmarks::BookmarkNode* folder1 = bookmarkModel->AddFolder(
      bookmarkModel->mobile_node(), 0, base::SysNSStringToUTF16(folderTitle));
  folderTitle = @"Folder 1.1";
  bookmarkModel->AddFolder(bookmarkModel->mobile_node(), 0,
                           base::SysNSStringToUTF16(folderTitle));

  folderTitle = @"Folder 2";
  const bookmarks::BookmarkNode* folder2 = bookmarkModel->AddFolder(
      folder1, 0, base::SysNSStringToUTF16(folderTitle));

  folderTitle = @"Folder 3";
  const bookmarks::BookmarkNode* folder3 = bookmarkModel->AddFolder(
      folder2, 0, base::SysNSStringToUTF16(folderTitle));

  NSString* thirdTitle = @"Third URL";
  const GURL fourthGURL = GURL(base::SysNSStringToUTF8(fourthURL));
  bookmarkModel->AddURL(folder3, 0, base::SysNSStringToUTF16(thirdTitle),
                        fourthGURL);
  return nil;
}

+ (NSError*)setupBookmarksWhichExceedsScreenHeightUsingURL:(NSString*)URL
                                                 inStorage:
                                                     (bookmarks::StorageType)
                                                         storageType {
  NSError* waitForBookmarkModelsLoadedError =
      [BookmarkEarlGreyAppInterface waitForBookmarkModelsLoaded];
  if (waitForBookmarkModelsLoadedError) {
    return waitForBookmarkModelsLoadedError;
  }
  bookmarks::BookmarkModel* bookmarkModel =
      [BookmarkEarlGreyAppInterface bookmarkModelOfStorage:storageType];

  const GURL dummyURL = GURL(base::SysNSStringToUTF8(URL));
  bookmarkModel->AddURL(bookmarkModel->mobile_node(), 0,
                        base::SysNSStringToUTF16(@"Bottom URL"), dummyURL);

  NSString* dummyTitle = @"Dummy URL";
  for (int i = 0; i < 20; i++) {
    bookmarkModel->AddURL(bookmarkModel->mobile_node(), 0,
                          base::SysNSStringToUTF16(dummyTitle), dummyURL);
  }
  NSString* folderTitle = @"Folder 1";
  const bookmarks::BookmarkNode* folder1 = bookmarkModel->AddFolder(
      bookmarkModel->mobile_node(), 0, base::SysNSStringToUTF16(folderTitle));
  bookmarkModel->AddURL(bookmarkModel->mobile_node(), 0,
                        base::SysNSStringToUTF16(@"Top URL"), dummyURL);

  // Add URLs to Folder 1.
  bookmarkModel->AddURL(folder1, 0, base::SysNSStringToUTF16(dummyTitle),
                        dummyURL);
  bookmarkModel->AddURL(folder1, 0, base::SysNSStringToUTF16(@"Bottom 1"),
                        dummyURL);
  for (int i = 0; i < 20; i++) {
    bookmarkModel->AddURL(folder1, 0, base::SysNSStringToUTF16(dummyTitle),
                          dummyURL);
  }
  return nil;
}

+ (NSError*)waitForBookmarkModelsLoaded {
  bookmarks::BookmarkModel* localOrSyncableBookmarkModel =
      [BookmarkEarlGreyAppInterface localOrSyncableBookmarkModel];

  BOOL localOrSyncableModelSuccess =
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, ^{
            return localOrSyncableBookmarkModel->loaded();
          });
  if (!localOrSyncableModelSuccess) {
    return testing::NSErrorWithLocalizedDescription(
        @"Local/Syncable bookmark model did not load");
  }
  bookmarks::BookmarkModel* accountBookmarkModel =
      [BookmarkEarlGreyAppInterface accountBookmarkModel];

  if (!accountBookmarkModel) {
    return nil;
  }
  BOOL accountSuccess = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return accountBookmarkModel->loaded();
      });
  if (!accountSuccess) {
    return testing::NSErrorWithLocalizedDescription(
        @"Account bookmark model exists but did not load");
  }
  return nil;
}

+ (NSError*)verifyBookmarksWithTitle:(NSString*)title
                       expectedCount:(NSUInteger)expectedCount
                           inStorage:(bookmarks::StorageType)storageType {
  // Get BookmarkModel and wait for it to be loaded.
  bookmarks::BookmarkModel* bookmarkModel =
      [BookmarkEarlGreyAppInterface bookmarkModelOfStorage:storageType];

  // Verify the correct number of bookmarks exist.
  std::u16string matchString = base::SysNSStringToUTF16(title);
  int const kMaxCountOfBookmarks = 50;
  std::vector<bookmarks::TitledUrlMatch> matches =
      bookmarkModel->GetBookmarksMatching(
          matchString, kMaxCountOfBookmarks,
          query_parser::MatchingAlgorithm::DEFAULT);
  if (matches.size() != expectedCount) {
    return testing::NSErrorWithLocalizedDescription(
        @"Unexpected number of bookmarks");
  }

  return nil;
}

+ (NSError*)addBookmarkWithTitle:(NSString*)title
                             URL:(NSString*)url
                       inStorage:(bookmarks::StorageType)storageType {
  NSError* waitForBookmarkModelsLoadedError =
      [BookmarkEarlGreyAppInterface waitForBookmarkModelsLoaded];
  if (waitForBookmarkModelsLoadedError) {
    return waitForBookmarkModelsLoadedError;
  }

  GURL bookmarkURL = GURL(base::SysNSStringToUTF8(url));
  bookmarks::BookmarkModel* bookmarkModel =
      [BookmarkEarlGreyAppInterface bookmarkModelOfStorage:storageType];
  bookmarkModel->AddNewURL(bookmarkModel->mobile_node(), 0,
                           base::SysNSStringToUTF16(title), bookmarkURL);

  return nil;
}

+ (NSError*)removeBookmarkWithTitle:(NSString*)title
                          inStorage:(bookmarks::StorageType)storageType {
  std::u16string name16(base::SysNSStringToUTF16(title));
  bookmarks::BookmarkModel* bookmarkModel =
      [BookmarkEarlGreyAppInterface bookmarkModelOfStorage:storageType];
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->GetTitle() == name16) {
      bookmarkModel->Remove(bookmark,
                            bookmarks::metrics::BookmarkEditSource::kUser);
      return nil;
    }
  }
  return testing::NSErrorWithLocalizedDescription([NSString
      stringWithFormat:@"Could not remove bookmark with name %@", title]);
}

+ (NSError*)moveBookmarkWithTitle:(NSString*)bookmarkTitle
                toFolderWithTitle:(NSString*)newFolder
                        inStorage:(bookmarks::StorageType)storageType {
  std::u16string name16(base::SysNSStringToUTF16(bookmarkTitle));
  bookmarks::BookmarkModel* bookmarkModel =
      [BookmarkEarlGreyAppInterface bookmarkModelOfStorage:storageType];
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());
  const bookmarks::BookmarkNode* bookmark = iterator.Next();
  while (iterator.has_next()) {
    if (bookmark->GetTitle() == name16) {
      break;
    }
    bookmark = iterator.Next();
  }

  std::u16string folderName16(base::SysNSStringToUTF16(newFolder));
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iteratorFolder(
      bookmarkModel->root_node());
  const bookmarks::BookmarkNode* folder = iteratorFolder.Next();
  while (iteratorFolder.has_next()) {
    if (folder->GetTitle() == folderName16) {
      break;
    }
    folder = iteratorFolder.Next();
  }
  std::vector<const bookmarks::BookmarkNode*> toMove{bookmark};
  bookmarks::BookmarkModel* accountBookmarkModel =
      [BookmarkEarlGreyAppInterface accountBookmarkModel];
  if (!bookmark_utils_ios::MoveBookmarks(toMove, bookmarkModel,
                                         accountBookmarkModel, folder)) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"Could not move bookmark with name %@",
                                   bookmarkTitle]);
  }

  return nil;
}

+ (NSError*)verifyChildCount:(size_t)count
            inFolderWithName:(NSString*)name
                   inStorage:(bookmarks::StorageType)storageType {
  std::u16string name16(base::SysNSStringToUTF16(name));
  bookmarks::BookmarkModel* bookmarkModel =
      [BookmarkEarlGreyAppInterface bookmarkModelOfStorage:storageType];

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());

  const bookmarks::BookmarkNode* folder = nullptr;
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_folder() && bookmark->GetTitle() == name16) {
      folder = bookmark;
      break;
    }
  }
  if (!folder) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"No folder named %@", name]);
  }

  if (folder->children().size() != count) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:
                      @"Unexpected number of children in folder '%@': %" PRIuS
                       " instead of %" PRIuS,
                      name, folder->children().size(), count]);
  }

  return nil;
}

+ (NSError*)verifyExistenceOfBookmarkWithURL:(NSString*)URL
                                        name:(NSString*)name
                                   inStorage:
                                       (bookmarks::StorageType)storageType {
  const bookmarks::BookmarkNode* bookmark =
      [self bookmarkModelOfStorage:storageType]
          ->GetMostRecentlyAddedUserNodeForURL(
              GURL(base::SysNSStringToUTF16(URL)));
  if (!bookmark ||
      bookmark->GetTitle().compare(base::SysNSStringToUTF16(name)) != 0) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"Could not find bookmark named %@ for %@",
                                   name, URL]);
  }

  return nil;
}

+ (NSError*)verifyAbsenceOfBookmarkWithURL:(NSString*)URL
                                 inStorage:(bookmarks::StorageType)storageType {
  const bookmarks::BookmarkNode* bookmark =
      [self bookmarkModelOfStorage:storageType]
          ->GetMostRecentlyAddedUserNodeForURL(
              GURL(base::SysNSStringToUTF16(URL)));
  if (bookmark) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"There is a bookmark for %@", URL]);
  }

  return nil;
}

+ (NSError*)verifyExistenceOfFolderWithTitle:(NSString*)title
                                   inStorage:
                                       (bookmarks::StorageType)storageType {
  std::u16string folderTitle16(base::SysNSStringToUTF16(title));

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      [self bookmarkModelOfStorage:storageType]->root_node());
  BOOL folderExists = NO;

  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_url()) {
      continue;
    }
    // This is a folder.
    if (bookmark->GetTitle() == folderTitle16) {
      // Folder exists, return.
      return nil;
    }
  }

  if (!folderExists) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"Folder %@ doesn't exist", title]);
  }

  return nil;
}

+ (NSError*)verifyPromoAlreadySeen:(BOOL)seen {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browserState->GetPrefs();
  if (prefs->GetBoolean(prefs::kIosBookmarkPromoAlreadySeen) == seen) {
    return nil;
  }
  NSString* errorDescription =
      seen ? @"Expected promo already seen, but it wasn't."
           : @"Expected promo not already seen, but it was.";
  return testing::NSErrorWithLocalizedDescription(errorDescription);
}

+ (void)setPromoAlreadySeen:(BOOL)seen {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetBoolean(prefs::kIosBookmarkPromoAlreadySeen, seen);
}

+ (void)setPromoAlreadySeenNumberOfTimes:(int)times {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->SetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount, times);
}

+ (int)numberOfTimesPromoAlreadySeen {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  return prefs->GetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount);
}
#pragma mark - Helpers

+ (bookmarks::BookmarkModel*)localOrSyncableBookmarkModel {
  return ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState());
}

+ (bookmarks::BookmarkModel*)accountBookmarkModel {
  return ios::AccountBookmarkModelFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState());
}

+ (bookmarks::BookmarkModel*)bookmarkModelOfStorage:
    (bookmarks::StorageType)storageType {
  switch (storageType) {
    case bookmarks::StorageType::kLocalOrSyncable:
      return [BookmarkEarlGreyAppInterface localOrSyncableBookmarkModel];
    case bookmarks::StorageType::kAccount:
      return [BookmarkEarlGreyAppInterface accountBookmarkModel];
  }
}

@end
