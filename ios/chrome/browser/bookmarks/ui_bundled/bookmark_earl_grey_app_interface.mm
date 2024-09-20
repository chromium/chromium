// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_app_interface.h"

#import <vector>

#import "base/format_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/bookmarks/browser/titled_url_match.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/prefs/pref_service.h"
#import "components/query_parser/query_parser.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_path_cache.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/nserror_util.h"
#import "ui/base/models/tree_node_iterator.h"
#import "url/gurl.h"

namespace {

bookmarks::BookmarkModel* GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForProfile(
      chrome_test_util::GetOriginalProfile());
}

std::vector<const bookmarks::BookmarkNode*> GetBookmarksWithTitle(
    NSString* title,
    bookmarks::BookmarkModel* bookmark_model,
    BookmarkStorageType type) {
  int const kMaxCountOfBookmarks = 50;

  bookmarks::QueryFields query;
  query.title =
      std::make_unique<std::u16string>(base::SysNSStringToUTF16(title));

  std::vector<const bookmarks::BookmarkNode*> nodes =
      bookmarks::GetBookmarksMatchingProperties(bookmark_model, query,
                                                kMaxCountOfBookmarks);

  base::ranges::remove_if(nodes, [=](const bookmarks::BookmarkNode* node) {
    return type !=
           bookmark_utils_ios::GetBookmarkStorageType(node, bookmark_model);
  });

  return nodes;
}

const bookmarks::BookmarkNode* GetFirstBookmarkWithTitle(
    NSString* title,
    bookmarks::BookmarkModel* bookmark_model,
    BookmarkStorageType type) {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GetBookmarksWithTitle(title, bookmark_model, type);
  return nodes.empty() ? nullptr : nodes[0];
}

const bookmarks::BookmarkNode* GetMobileNodeWithType(
    BookmarkStorageType type,
    const bookmarks::BookmarkModel* model) {
  CHECK(model);
  switch (type) {
    case BookmarkStorageType::kLocalOrSyncable:
      return model->mobile_node();
    case BookmarkStorageType::kAccount:
      return model->account_mobile_node();
  }
  NOTREACHED();
}

}  // namespace

@implementation BookmarkEarlGreyAppInterface

#pragma mark - Public Interface

+ (NSError*)clearBookmarks {
  NSError* bookmarkModelLoadedError =
      [BookmarkEarlGreyAppInterface waitForBookmarkModelLoaded];
  if (bookmarkModelLoadedError) {
    return bookmarkModelLoadedError;
  }

  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:profile->GetPrefs()];

  bookmarkModel->RemoveAllUserBookmarks(FROM_HERE);
  ResetLastUsedBookmarkFolder(profile->GetPrefs());

  // Checking whether managed bookmarks remain, in which case return false.
  if (bookmarkModel->HasBookmarks()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Bookmark model is not empty. May have managed bookmarks.");
  }
  return nil;
}

+ (void)clearBookmarksPositionCache {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:profile->GetPrefs()];
}

+ (NSError*)setupStandardBookmarksUsingFirstURL:(NSString*)firstURL
                                      secondURL:(NSString*)secondURL
                                       thirdURL:(NSString*)thirdURL
                                      fourthURL:(NSString*)fourthURL
                                      inStorage:
                                          (BookmarkStorageType)storageType {
  NSError* bookmarkModelLoadedError =
      [BookmarkEarlGreyAppInterface waitForBookmarkModelLoaded];
  if (bookmarkModelLoadedError) {
    return bookmarkModelLoadedError;
  }
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();

  NSString* firstTitle = @"First URL";
  const GURL firstGURL = GURL(base::SysNSStringToUTF8(firstURL));
  bookmarkModel->AddURL(GetMobileNodeWithType(storageType, bookmarkModel), 0,
                        base::SysNSStringToUTF16(firstTitle), firstGURL);

  NSString* secondTitle = @"Second URL";
  const GURL secondGURL = GURL(base::SysNSStringToUTF8(secondURL));
  bookmarkModel->AddURL(GetMobileNodeWithType(storageType, bookmarkModel), 0,
                        base::SysNSStringToUTF16(secondTitle), secondGURL);

  NSString* frenchTitle = @"French URL";
  const GURL thirdGURL = GURL(base::SysNSStringToUTF8(thirdURL));
  bookmarkModel->AddURL(GetMobileNodeWithType(storageType, bookmarkModel), 0,
                        base::SysNSStringToUTF16(frenchTitle), thirdGURL);

  NSString* folderTitle = @"Folder 1";
  const bookmarks::BookmarkNode* folder1 = bookmarkModel->AddFolder(
      GetMobileNodeWithType(storageType, bookmarkModel), 0,
      base::SysNSStringToUTF16(folderTitle));
  folderTitle = @"Folder 1.1";
  bookmarkModel->AddFolder(GetMobileNodeWithType(storageType, bookmarkModel), 0,
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
                                                 inStorage:(BookmarkStorageType)
                                                               storageType {
  NSError* waitForBookmarkModelLoadedError =
      [BookmarkEarlGreyAppInterface waitForBookmarkModelLoaded];
  if (waitForBookmarkModelLoadedError) {
    return waitForBookmarkModelLoadedError;
  }
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();

  const GURL dummyURL = GURL(base::SysNSStringToUTF8(URL));
  bookmarkModel->AddURL(GetMobileNodeWithType(storageType, bookmarkModel), 0,
                        base::SysNSStringToUTF16(@"Bottom URL"), dummyURL);

  NSString* dummyTitle = @"Dummy URL";
  for (int i = 0; i < 20; i++) {
    bookmarkModel->AddURL(GetMobileNodeWithType(storageType, bookmarkModel), 0,
                          base::SysNSStringToUTF16(dummyTitle), dummyURL);
  }
  NSString* folderTitle = @"Folder 1";
  const bookmarks::BookmarkNode* folder1 = bookmarkModel->AddFolder(
      GetMobileNodeWithType(storageType, bookmarkModel), 0,
      base::SysNSStringToUTF16(folderTitle));
  bookmarkModel->AddURL(GetMobileNodeWithType(storageType, bookmarkModel), 0,
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

+ (NSError*)waitForBookmarkModelLoaded {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();

  if (!base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, ^{
            return bookmarkModel->loaded();
          })) {
    return testing::NSErrorWithLocalizedDescription(
        @"Bookmark model did not load");
  }
  return nil;
}

+ (void)commitPendingWrite {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();
  bookmarkModel->CommitPendingWriteForTest();
}

+ (void)setLastUsedBookmarkFolderToMobileBookmarksInStorageType:
    (BookmarkStorageType)storageType {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();
  const bookmarks::BookmarkNode* folder =
      GetMobileNodeWithType(storageType, bookmarkModel);
  SetLastUsedBookmarkFolder(chrome_test_util::GetOriginalProfile()->GetPrefs(),
                            folder, storageType);
}

+ (NSError*)verifyBookmarksWithTitle:(NSString*)title
                       expectedCount:(NSUInteger)expectedCount
                           inStorage:(BookmarkStorageType)storageType {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();

  std::vector<const bookmarks::BookmarkNode*> nodes =
      GetBookmarksWithTitle(title, bookmarkModel, storageType);

  if (nodes.size() != expectedCount) {
    return testing::NSErrorWithLocalizedDescription(
        @"Unexpected number of bookmarks");
  }

  return nil;
}

+ (NSError*)addBookmarkWithTitle:(NSString*)title
                             URL:(NSString*)url
                       inStorage:(BookmarkStorageType)storageType {
  NSError* waitForBookmarkModelLoadedError =
      [BookmarkEarlGreyAppInterface waitForBookmarkModelLoaded];
  if (waitForBookmarkModelLoadedError) {
    return waitForBookmarkModelLoadedError;
  }

  GURL bookmarkURL = GURL(base::SysNSStringToUTF8(url));
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();
  bookmarkModel->AddNewURL(GetMobileNodeWithType(storageType, bookmarkModel), 0,
                           base::SysNSStringToUTF16(title), bookmarkURL);

  return nil;
}

+ (NSError*)removeBookmarkWithTitle:(NSString*)title
                          inStorage:(BookmarkStorageType)storageType {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();
  const bookmarks::BookmarkNode* bookmark =
      GetFirstBookmarkWithTitle(title, bookmarkModel, storageType);
  if (!bookmark) {
    return testing::NSErrorWithLocalizedDescription([NSString
        stringWithFormat:@"Could not remove bookmark with name %@", title]);
  }

  bookmarkModel->Remove(bookmark, bookmarks::metrics::BookmarkEditSource::kUser,
                        FROM_HERE);
  return nil;
}

+ (NSError*)moveBookmarkWithTitle:(NSString*)bookmarkTitle
                toFolderWithTitle:(NSString*)newFolderTitle
                        inStorage:(BookmarkStorageType)storageType {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();

  const bookmarks::BookmarkNode* bookmark =
      GetFirstBookmarkWithTitle(bookmarkTitle, bookmarkModel, storageType);
  if (!bookmark) {
    return testing::NSErrorWithLocalizedDescription([NSString
        stringWithFormat:@"Could not move inexistent bookmark with title %@",
                         bookmarkTitle]);
  }

  const bookmarks::BookmarkNode* newFolder =
      GetFirstBookmarkWithTitle(newFolderTitle, bookmarkModel, storageType);
  if (!newFolder || !newFolder->is_folder()) {
    return testing::NSErrorWithLocalizedDescription([NSString
        stringWithFormat:
            @"Could not move bookmark to inexistent folder with title %@",
            newFolderTitle]);
  }

  std::vector<const bookmarks::BookmarkNode*> toMove{bookmark};
  if (!bookmark_utils_ios::MoveBookmarks(toMove, bookmarkModel, newFolder)) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"Could not move bookmark with name %@",
                                   bookmarkTitle]);
  }

  return nil;
}

+ (NSError*)verifyChildCount:(size_t)count
            inFolderWithName:(NSString*)name
                   inStorage:(BookmarkStorageType)storageType {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();

  const bookmarks::BookmarkNode* folder =
      GetFirstBookmarkWithTitle(name, bookmarkModel, storageType);
  if (!folder || !folder->is_folder()) {
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
                                   inStorage:(BookmarkStorageType)storageType {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();

  for (const bookmarks::BookmarkNode* bookmark :
       bookmarkModel->GetNodesByURL(GURL(base::SysNSStringToUTF16(URL)))) {
    if (bookmark_utils_ios::GetBookmarkStorageType(bookmark, bookmarkModel) ==
            storageType &&
        bookmark->GetTitle().compare(base::SysNSStringToUTF16(name)) == 0) {
      return nil;
    }
  }

  return testing::NSErrorWithLocalizedDescription([NSString
      stringWithFormat:@"Could not find bookmark named %@ for %@", name, URL]);
}

+ (NSError*)verifyAbsenceOfBookmarkWithURL:(NSString*)URL
                                 inStorage:(BookmarkStorageType)storageType {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();

  for (const bookmarks::BookmarkNode* bookmark :
       bookmarkModel->GetNodesByURL(GURL(base::SysNSStringToUTF16(URL)))) {
    if (bookmark_utils_ios::GetBookmarkStorageType(bookmark, bookmarkModel) ==
        storageType) {
      return testing::NSErrorWithLocalizedDescription(
          [NSString stringWithFormat:@"There is a bookmark for %@", URL]);
    }
  }

  return nil;
}

+ (NSError*)verifyExistenceOfFolderWithTitle:(NSString*)title
                                   inStorage:(BookmarkStorageType)storageType {
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModel();
  const bookmarks::BookmarkNode* folder =
      GetFirstBookmarkWithTitle(title, bookmarkModel, storageType);
  if (!folder || !folder->is_folder()) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"Folder %@ doesn't exist", title]);
  }

  return nil;
}

+ (NSError*)verifyPromoAlreadySeen:(BOOL)seen {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  PrefService* prefs = profile->GetPrefs();
  if (prefs->GetBoolean(prefs::kIosBookmarkPromoAlreadySeen) == seen) {
    return nil;
  }
  NSString* errorDescription =
      seen ? @"Expected promo already seen, but it wasn't."
           : @"Expected promo not already seen, but it was.";
  return testing::NSErrorWithLocalizedDescription(errorDescription);
}

+ (void)setPromoAlreadySeen:(BOOL)seen {
  PrefService* prefs = chrome_test_util::GetOriginalProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kIosBookmarkPromoAlreadySeen, seen);
}

+ (void)setPromoAlreadySeenNumberOfTimes:(int)times {
  PrefService* prefs = chrome_test_util::GetOriginalProfile()->GetPrefs();
  prefs->SetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount, times);
}

+ (int)numberOfTimesPromoAlreadySeen {
  PrefService* prefs = chrome_test_util::GetOriginalProfile()->GetPrefs();
  return prefs->GetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount);
}

@end
