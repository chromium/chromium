// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/task_environment.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "components/bookmarks/test/test_bookmark_client.h"
#import "components/favicon/core/large_icon_service_impl.h"
#import "components/favicon/core/test/mock_favicon_service.h"
#import "ios/chrome/app/spotlight/fake_searchable_item_factory.h"
#import "ios/chrome/app/spotlight/fake_spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/skia/include/core/SkBitmap.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

using testing::_;
using ui::test::uiimage_utils::UIImageWithSizeAndSolidColor;

namespace {
const char kDummyIconUrl[] = "http://www.example.com/touch_icon.png";

favicon_base::FaviconRawBitmapResult CreateTestBitmap(int w, int h) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  CGSize size = CGSizeMake(w, h);
  UIImage* favicon = UIImageWithSizeAndSolidColor(size, [UIColor redColor]);
  NSData* png = UIImagePNGRepresentation(favicon);
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes(
      static_cast<const unsigned char*>([png bytes]), [png length]));

  result.bitmap_data = data;
  result.pixel_size = gfx::Size(w, h);
  result.icon_url = GURL(kDummyIconUrl);
  result.icon_type = favicon_base::IconType::kTouchIcon;
  CHECK(result.is_valid());
  return result;
}

}  // namespace

class BookmarkSpotlightManagerTest : public BookmarkIOSUnitTestSupport {
 public:
  BookmarkSpotlightManagerTest() {
    CreateMockLargeIconService();
    spotlightInterface_ = [[FakeSpotlightInterface alloc] init];
    searchableItemFactory_ = [[FakeSearchableItemFactory alloc]
        initWithDomain:spotlight::DOMAIN_BOOKMARKS];
  }

 protected:
  void CreateMockLargeIconService() {
    large_icon_service_.reset(new favicon::LargeIconServiceImpl(
        &mock_favicon_service_, /*image_fetcher=*/nullptr,
        /*desired_size_in_dip_for_server_requests=*/0,
        /*icon_type_for_server_requests=*/favicon_base::IconType::kTouchIcon,
        /*google_server_client_param=*/"test_chrome"));

    EXPECT_CALL(mock_favicon_service_,
                GetLargestRawFaviconForPageURL(_, _, _, _, _))
        .WillRepeatedly([](auto, auto, auto,
                           favicon_base::FaviconRawBitmapCallback callback,
                           base::CancelableTaskTracker* tracker) {
          return tracker->PostTask(
              base::SingleThreadTaskRunner::GetCurrentDefault().get(),
              FROM_HERE,
              base::BindOnce(std::move(callback), CreateTestBitmap(24, 24)));
        });
  }

  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<favicon::LargeIconServiceImpl> large_icon_service_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  FakeSpotlightInterface* spotlightInterface_;
  FakeSearchableItemFactory* searchableItemFactory_;
};

/// Tests that clearAndReindexModel actually clears all bookmarks items and
/// attempt to reindex the existing items in bookmark.
TEST_F(BookmarkSpotlightManagerTest, testClearAndReindexModel) {
  AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"foo1",
              GURL("http://foo1.com"));
  AddBookmark(account_bookmark_model_->mobile_node(), u"foo2",
              GURL("http://foo2.com"));

  FakeSpotlightInterface* fakeSpotlightInterface =
      [[FakeSpotlightInterface alloc] init];

  BookmarksSpotlightManager* manager = [[BookmarksSpotlightManager alloc]
          initWithLargeIconService:large_icon_service_.get()
      localOrSyncableBookmarkModel:local_or_syncable_bookmark_model_
              accountBookmarkModel:account_bookmark_model_
                spotlightInterface:fakeSpotlightInterface
             searchableItemFactory:searchableItemFactory_];

  NSUInteger initialIndexedItemCount =
      fakeSpotlightInterface.indexSearchableItemsCallsCount;

  [manager clearAndReindexModel];

  // We expect to attempt deleting searchable items.
  EXPECT_EQ(fakeSpotlightInterface
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            1u);

  // We expect that we will reindex the only existing item in bookmark, thus the
  // +2 for the count.
  EXPECT_EQ(fakeSpotlightInterface.indexSearchableItemsCallsCount,
            initialIndexedItemCount + 2);

  [manager shutdown];
}

/// Tests that when calling parentFolderNamesForNode giving a bookmark node, it
/// returns an array of its ancestor folder names
TEST_F(BookmarkSpotlightManagerTest, testParentFolderNamesForNode) {
  BookmarksSpotlightManager* manager = [[BookmarksSpotlightManager alloc]
          initWithLargeIconService:large_icon_service_.get()
      localOrSyncableBookmarkModel:local_or_syncable_bookmark_model_
              accountBookmarkModel:account_bookmark_model_
                spotlightInterface:spotlightInterface_
             searchableItemFactory:searchableItemFactory_];

  const bookmarks::BookmarkNode* root =
      local_or_syncable_bookmark_model_->mobile_node();
  static const std::string model_string("a 1:[ b c ] d 2:[ 21:[ e ] f g ] h ");
  bookmarks::test::AddNodesFromModelString(local_or_syncable_bookmark_model_,
                                           root, model_string);
  const bookmarks::BookmarkNode* eNode =
      root->children()[3]->children().front()->children().front().get();
  NSMutableArray* folderNames = [manager parentFolderNamesForNode:eNode];

  EXPECT_EQ([folderNames count], 2u);
  EXPECT_TRUE([[folderNames objectAtIndex:0] isEqualToString:@"2"]);
  EXPECT_TRUE([[folderNames objectAtIndex:1] isEqualToString:@"21"]);

  [manager shutdown];
}

/// Tests that when we add a new bookmark item, we actually try to
/// index/refresh it in spotlight.
TEST_F(BookmarkSpotlightManagerTest, testRefreshItemWithURL) {
  FakeSpotlightInterface* fakeSpotlightInterface =
      [[FakeSpotlightInterface alloc] init];

  BookmarksSpotlightManager* manager = [[BookmarksSpotlightManager alloc]
          initWithLargeIconService:large_icon_service_.get()
      localOrSyncableBookmarkModel:local_or_syncable_bookmark_model_
              accountBookmarkModel:account_bookmark_model_
                spotlightInterface:fakeSpotlightInterface
             searchableItemFactory:searchableItemFactory_];

  NSUInteger initialIndexedItemCount =
      fakeSpotlightInterface.indexSearchableItemsCallsCount;

  AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"foo1",
              GURL("http://foo1.com"));
  AddBookmark(account_bookmark_model_->mobile_node(), u"foo2",
              GURL("http://foo2.com"));

  // We expect to call indexSearchableItems api method to add the new added
  // bookmark items.
  EXPECT_EQ(fakeSpotlightInterface.indexSearchableItemsCallsCount,
            initialIndexedItemCount + 2);

  [manager shutdown];
}

/// Tests that when we update a bookmark item, we actually try to remove it from
/// spotlight and reindex it.
TEST_F(BookmarkSpotlightManagerTest, testUpdateBookmarkItem) {
  FakeSpotlightInterface* fakeSpotlightInterface =
      [[FakeSpotlightInterface alloc] init];

  BookmarksSpotlightManager* manager = [[BookmarksSpotlightManager alloc]
          initWithLargeIconService:large_icon_service_.get()
      localOrSyncableBookmarkModel:local_or_syncable_bookmark_model_
              accountBookmarkModel:account_bookmark_model_
                spotlightInterface:fakeSpotlightInterface
             searchableItemFactory:searchableItemFactory_];

  NSUInteger currentIndexedItemCount =
      fakeSpotlightInterface.indexSearchableItemsCallsCount;

  const bookmarks::BookmarkNode* addedNode1 =
      AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"foo1",
                  GURL("http://foo1.com"));
  const bookmarks::BookmarkNode* addedNode2 = AddBookmark(
      account_bookmark_model_->mobile_node(), u"foo2", GURL("http://foo2.com"));

  // We expect to call indexSearchableItems api method to add the new added
  // bookmark item.
  EXPECT_EQ(fakeSpotlightInterface.indexSearchableItemsCallsCount,
            currentIndexedItemCount + 2);

  currentIndexedItemCount =
      fakeSpotlightInterface.indexSearchableItemsCallsCount;

  local_or_syncable_bookmark_model_->SetTitle(
      addedNode1, u"new title 1",
      bookmarks::metrics::BookmarkEditSource::kOther);
  account_bookmark_model_->SetTitle(
      addedNode2, u"new title 2",
      bookmarks::metrics::BookmarkEditSource::kOther);

  // We expect to delete the modified item using its identifier.
  EXPECT_EQ(
      fakeSpotlightInterface.deleteSearchableItemsWithIdentifiersCallsCount,
      2u);

  // We expect reindexing it with the new details.
  EXPECT_EQ(fakeSpotlightInterface.indexSearchableItemsCallsCount,
            currentIndexedItemCount + 2);

  [manager shutdown];
}
