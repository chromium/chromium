// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_with_shared_underlying_model.h"

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ios {

namespace {

using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::SizeIs;

}  // namespace

class LegacyBookmarkModelWithSharedUnderlyingModelTest : public testing::Test {
 protected:
  LegacyBookmarkModelWithSharedUnderlyingModelTest()
      : shared_model_(bookmarks::TestBookmarkClient::CreateModel()),
        local_view_(/*underlying_model=*/shared_model_.get(),
                    bookmarks::BookmarkModel::NodeTypeForUuidLookup::
                        kLocalOrSyncableNodes,
                    /*managed_bookmark_service=*/nullptr),
        account_view_(
            /*underlying_model=*/shared_model_.get(),
            bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes,
            /*managed_bookmark_service=*/nullptr) {
    local_view_.AddObserver(&local_view_observer_);
    account_view_.AddObserver(&account_view_observer_);
  }

  ~LegacyBookmarkModelWithSharedUnderlyingModelTest() override {
    local_view_.RemoveObserver(&local_view_observer_);
    account_view_.RemoveObserver(&account_view_observer_);
  }

  void PopulateTestNodes() {
    shared_model_->AddURL(shared_model_->bookmark_bar_node(), 0, kLocalTitle1,
                          kLocalUrl1);
    shared_model_->AddURL(shared_model_->other_node(), 0, kLocalTitle2,
                          kLocalUrl2);
    shared_model_->AddURL(shared_model_->mobile_node(), 0, kLocalTitle3,
                          kLocalUrl3);

    shared_model_->CreateAccountPermanentFolders();
    shared_model_->AddURL(shared_model_->account_bookmark_bar_node(), 0,
                          kAccountTitle1, kAccountUrl1);
    shared_model_->AddURL(shared_model_->account_other_node(), 0,
                          kAccountTitle2, kAccountUrl2);
    shared_model_->AddURL(shared_model_->account_mobile_node(), 0,
                          kAccountTitle3, kAccountUrl3);
  }

  const GURL kLocalUrl1 = GURL("https://local1.com/");
  const GURL kLocalUrl2 = GURL("https://local2.com/");
  const GURL kLocalUrl3 = GURL("https://local3.com/");
  const std::u16string kLocalTitle1 = u"LocalTitle1";
  const std::u16string kLocalTitle2 = u"LocalTitle2";
  const std::u16string kLocalTitle3 = u"LocalTitle3";
  const GURL kAccountUrl1 = GURL("https://account1.com/");
  const GURL kAccountUrl2 = GURL("https://account2.com/");
  const GURL kAccountUrl3 = GURL("https://account3.com/");
  const std::u16string kAccountTitle1 = u"AccountTitle1";
  const std::u16string kAccountTitle2 = u"AccountTitle2";
  const std::u16string kAccountTitle3 = u"AccountTitle3";

  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kEnableBookmarkFoldersForAccountStorage};
  std::unique_ptr<bookmarks::BookmarkModel> shared_model_;
  LegacyBookmarkModelWithSharedUnderlyingModel local_view_;
  LegacyBookmarkModelWithSharedUnderlyingModel account_view_;
  testing::NiceMock<bookmarks::MockBookmarkModelObserver> local_view_observer_;
  testing::NiceMock<bookmarks::MockBookmarkModelObserver>
      account_view_observer_;
};

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest, IsBookmarked) {
  PopulateTestNodes();

  ASSERT_TRUE(shared_model_->IsBookmarked(kLocalUrl1));
  ASSERT_TRUE(shared_model_->IsBookmarked(kLocalUrl2));
  ASSERT_TRUE(shared_model_->IsBookmarked(kLocalUrl3));
  ASSERT_TRUE(shared_model_->IsBookmarked(kAccountUrl1));
  ASSERT_TRUE(shared_model_->IsBookmarked(kAccountUrl2));
  ASSERT_TRUE(shared_model_->IsBookmarked(kAccountUrl3));

  EXPECT_TRUE(local_view_.IsBookmarked(kLocalUrl1));
  EXPECT_TRUE(local_view_.IsBookmarked(kLocalUrl2));
  EXPECT_TRUE(local_view_.IsBookmarked(kLocalUrl3));
  EXPECT_FALSE(local_view_.IsBookmarked(kAccountUrl1));
  EXPECT_FALSE(local_view_.IsBookmarked(kAccountUrl2));
  EXPECT_FALSE(local_view_.IsBookmarked(kAccountUrl3));

  EXPECT_FALSE(account_view_.IsBookmarked(kLocalUrl1));
  EXPECT_FALSE(account_view_.IsBookmarked(kLocalUrl2));
  EXPECT_FALSE(account_view_.IsBookmarked(kLocalUrl3));
  EXPECT_TRUE(account_view_.IsBookmarked(kAccountUrl1));
  EXPECT_TRUE(account_view_.IsBookmarked(kAccountUrl2));
  EXPECT_TRUE(account_view_.IsBookmarked(kAccountUrl3));
}

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest, GetNodesByURL) {
  PopulateTestNodes();

  ASSERT_THAT(shared_model_->GetNodesByURL(kLocalUrl1), SizeIs(1));
  ASSERT_THAT(shared_model_->GetNodesByURL(kLocalUrl2), SizeIs(1));
  ASSERT_THAT(shared_model_->GetNodesByURL(kLocalUrl3), SizeIs(1));
  ASSERT_THAT(shared_model_->GetNodesByURL(kAccountUrl1), SizeIs(1));
  ASSERT_THAT(shared_model_->GetNodesByURL(kAccountUrl2), SizeIs(1));
  ASSERT_THAT(shared_model_->GetNodesByURL(kAccountUrl3), SizeIs(1));

  EXPECT_THAT(local_view_.GetNodesByURL(kLocalUrl1), SizeIs(1));
  EXPECT_THAT(local_view_.GetNodesByURL(kLocalUrl2), SizeIs(1));
  EXPECT_THAT(local_view_.GetNodesByURL(kLocalUrl3), SizeIs(1));
  EXPECT_THAT(local_view_.GetNodesByURL(kAccountUrl1), SizeIs(0));
  EXPECT_THAT(local_view_.GetNodesByURL(kAccountUrl2), SizeIs(0));
  EXPECT_THAT(local_view_.GetNodesByURL(kAccountUrl3), SizeIs(0));

  EXPECT_THAT(account_view_.GetNodesByURL(kLocalUrl1), SizeIs(0));
  EXPECT_THAT(account_view_.GetNodesByURL(kLocalUrl2), SizeIs(0));
  EXPECT_THAT(account_view_.GetNodesByURL(kLocalUrl3), SizeIs(0));
  EXPECT_THAT(account_view_.GetNodesByURL(kAccountUrl1), SizeIs(1));
  EXPECT_THAT(account_view_.GetNodesByURL(kAccountUrl2), SizeIs(1));
  EXPECT_THAT(account_view_.GetNodesByURL(kAccountUrl3), SizeIs(1));
}

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest, GetNodeById) {
  shared_model_->CreateAccountPermanentFolders();
  const bookmarks::BookmarkNode* local_node = shared_model_->AddURL(
      shared_model_->mobile_node(), 0, kLocalTitle1, kLocalUrl1);
  const bookmarks::BookmarkNode* account_node = shared_model_->AddURL(
      shared_model_->account_mobile_node(), 0, kAccountTitle1, kAccountUrl1);

  EXPECT_THAT(local_view_.GetNodeById(local_node->id()), Eq(local_node));
  EXPECT_THAT(local_view_.GetNodeById(account_node->id()), IsNull());

  EXPECT_THAT(account_view_.GetNodeById(local_node->id()), IsNull());
  EXPECT_THAT(account_view_.GetNodeById(account_node->id()), Eq(account_node));

  EXPECT_THAT(local_view_.GetNodeById(12345), IsNull());
  EXPECT_THAT(account_view_.GetNodeById(12345), IsNull());
}

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest, HasBookmarks) {
  shared_model_->CreateAccountPermanentFolders();

  ASSERT_FALSE(shared_model_->HasBookmarks());
  ASSERT_TRUE(shared_model_->HasNoUserCreatedBookmarksOrFolders());
  EXPECT_FALSE(local_view_.HasBookmarks());
  EXPECT_TRUE(local_view_.HasNoUserCreatedBookmarksOrFolders());
  EXPECT_FALSE(account_view_.HasBookmarks());
  EXPECT_TRUE(account_view_.HasNoUserCreatedBookmarksOrFolders());

  shared_model_->AddFolder(shared_model_->mobile_node(), 0, u"Local");

  ASSERT_FALSE(shared_model_->HasBookmarks());
  ASSERT_FALSE(shared_model_->HasNoUserCreatedBookmarksOrFolders());
  EXPECT_FALSE(local_view_.HasBookmarks());
  EXPECT_FALSE(local_view_.HasNoUserCreatedBookmarksOrFolders());
  EXPECT_FALSE(account_view_.HasBookmarks());
  EXPECT_TRUE(account_view_.HasNoUserCreatedBookmarksOrFolders());

  shared_model_->AddFolder(shared_model_->account_mobile_node(), 0, u"Account");

  ASSERT_FALSE(shared_model_->HasBookmarks());
  ASSERT_FALSE(shared_model_->HasNoUserCreatedBookmarksOrFolders());
  EXPECT_FALSE(local_view_.HasBookmarks());
  EXPECT_FALSE(local_view_.HasNoUserCreatedBookmarksOrFolders());
  EXPECT_FALSE(account_view_.HasBookmarks());
  EXPECT_FALSE(account_view_.HasNoUserCreatedBookmarksOrFolders());

  shared_model_->AddURL(shared_model_->mobile_node(), 0, kLocalTitle1,
                        kLocalUrl1);

  ASSERT_TRUE(shared_model_->HasBookmarks());
  ASSERT_FALSE(shared_model_->HasNoUserCreatedBookmarksOrFolders());
  EXPECT_TRUE(local_view_.HasBookmarks());
  EXPECT_FALSE(local_view_.HasNoUserCreatedBookmarksOrFolders());
  EXPECT_FALSE(account_view_.HasBookmarks());
  EXPECT_FALSE(account_view_.HasNoUserCreatedBookmarksOrFolders());

  shared_model_->AddURL(shared_model_->account_mobile_node(), 0, kAccountTitle1,
                        kAccountUrl1);

  ASSERT_TRUE(shared_model_->HasBookmarks());
  ASSERT_FALSE(shared_model_->HasNoUserCreatedBookmarksOrFolders());
  EXPECT_TRUE(local_view_.HasBookmarks());
  EXPECT_FALSE(local_view_.HasNoUserCreatedBookmarksOrFolders());
  EXPECT_TRUE(account_view_.HasBookmarks());
  EXPECT_FALSE(account_view_.HasNoUserCreatedBookmarksOrFolders());
}

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest,
       GetBookmarksMatchingProperties) {
  const size_t kMaxCount = 10;

  PopulateTestNodes();

  bookmarks::QueryFields query1;
  query1.title = std::make_unique<std::u16string>(kLocalTitle1);

  EXPECT_THAT(local_view_.GetBookmarksMatchingProperties(query1, kMaxCount),
              SizeIs(1));

  EXPECT_THAT(account_view_.GetBookmarksMatchingProperties(query1, kMaxCount),
              SizeIs(0));

  bookmarks::QueryFields query2;
  query2.title = std::make_unique<std::u16string>(kAccountTitle1);

  EXPECT_THAT(local_view_.GetBookmarksMatchingProperties(query2, kMaxCount),
              SizeIs(0));

  EXPECT_THAT(account_view_.GetBookmarksMatchingProperties(query2, kMaxCount),
              SizeIs(1));
}

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest,
       NotifyBookmarkNodeAdded) {
  shared_model_->CreateAccountPermanentFolders();

  ASSERT_THAT(local_view_.bookmark_bar_node(),
              Eq(shared_model_->bookmark_bar_node()));
  ASSERT_THAT(local_view_.mobile_node(), Eq(shared_model_->mobile_node()));
  ASSERT_THAT(local_view_.other_node(), Eq(shared_model_->other_node()));
  ASSERT_THAT(account_view_.bookmark_bar_node(),
              Eq(shared_model_->account_bookmark_bar_node()));
  ASSERT_THAT(account_view_.mobile_node(),
              Eq(shared_model_->account_mobile_node()));
  ASSERT_THAT(account_view_.other_node(),
              Eq(shared_model_->account_other_node()));

  EXPECT_CALL(local_view_observer_,
              BookmarkNodeAdded(shared_model_->bookmark_bar_node(), 0, false));
  EXPECT_CALL(account_view_observer_, BookmarkNodeAdded).Times(0);
  shared_model_->AddURL(shared_model_->bookmark_bar_node(), 0, kLocalTitle1,
                        kLocalUrl1);
  testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
  testing::Mock::VerifyAndClearExpectations(&account_view_observer_);

  EXPECT_CALL(local_view_observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(
      account_view_observer_,
      BookmarkNodeAdded(shared_model_->account_bookmark_bar_node(), 0, false));
  shared_model_->AddURL(shared_model_->account_bookmark_bar_node(), 0,
                        kAccountTitle1, kAccountUrl1);
  testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
  testing::Mock::VerifyAndClearExpectations(&account_view_observer_);
}

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest,
       NotifyBookmarkNodeRemoved) {
  shared_model_->CreateAccountPermanentFolders();

  const bookmarks::BookmarkNode* node1 = shared_model_->AddURL(
      shared_model_->bookmark_bar_node(), 0, kLocalTitle1, kLocalUrl1);
  const bookmarks::BookmarkNode* node2 = shared_model_->AddURL(
      shared_model_->account_bookmark_bar_node(), 0, kLocalTitle2, kLocalUrl2);

  {
    testing::InSequence seq;
    EXPECT_CALL(
        local_view_observer_,
        OnWillRemoveBookmarks(shared_model_->bookmark_bar_node(), 0, node1, _));
    EXPECT_CALL(local_view_observer_,
                BookmarkNodeRemoved(shared_model_->bookmark_bar_node(), 0,
                                    node1, _, _));
    EXPECT_CALL(account_view_observer_, OnWillRemoveBookmarks).Times(0);
    EXPECT_CALL(account_view_observer_, BookmarkNodeRemoved).Times(0);
    shared_model_->Remove(node1, bookmarks::metrics::BookmarkEditSource::kOther,
                          FROM_HERE);
    testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
    testing::Mock::VerifyAndClearExpectations(&account_view_observer_);
  }

  {
    testing::InSequence seq;
    EXPECT_CALL(account_view_observer_,
                OnWillRemoveBookmarks(
                    shared_model_->account_bookmark_bar_node(), 0, node2, _));
    EXPECT_CALL(account_view_observer_,
                BookmarkNodeRemoved(shared_model_->account_bookmark_bar_node(),
                                    0, node2, _, _));
    EXPECT_CALL(local_view_observer_, OnWillRemoveBookmarks).Times(0);
    EXPECT_CALL(local_view_observer_, BookmarkNodeRemoved).Times(0);
    shared_model_->Remove(node2, bookmarks::metrics::BookmarkEditSource::kOther,
                          FROM_HERE);
    testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
    testing::Mock::VerifyAndClearExpectations(&account_view_observer_);
  }
}

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest,
       NotifyBookmarkNodeChanged) {
  shared_model_->CreateAccountPermanentFolders();

  const bookmarks::BookmarkNode* node1 = shared_model_->AddURL(
      shared_model_->bookmark_bar_node(), 0, kLocalTitle1, kLocalUrl1);
  const bookmarks::BookmarkNode* node2 = shared_model_->AddURL(
      shared_model_->account_bookmark_bar_node(), 0, kLocalTitle2, kLocalUrl2);

  {
    testing::InSequence seq;
    EXPECT_CALL(local_view_observer_, OnWillChangeBookmarkNode(node1));
    EXPECT_CALL(local_view_observer_, BookmarkNodeChanged(node1));
    EXPECT_CALL(account_view_observer_, OnWillChangeBookmarkNode).Times(0);
    EXPECT_CALL(account_view_observer_, BookmarkNodeChanged).Times(0);
    shared_model_->SetTitle(node1, u"New Title",
                            bookmarks::metrics::BookmarkEditSource::kOther);
    testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
    testing::Mock::VerifyAndClearExpectations(&account_view_observer_);
  }

  {
    testing::InSequence seq;
    EXPECT_CALL(account_view_observer_, OnWillChangeBookmarkNode(node2));
    EXPECT_CALL(account_view_observer_, BookmarkNodeChanged(node2));
    EXPECT_CALL(local_view_observer_, OnWillChangeBookmarkNode).Times(0);
    EXPECT_CALL(local_view_observer_, BookmarkNodeChanged).Times(0);
    shared_model_->SetTitle(node2, u"New Title",
                            bookmarks::metrics::BookmarkEditSource::kOther);
    testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
    testing::Mock::VerifyAndClearExpectations(&account_view_observer_);
  }
}

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest,
       NotifyBookmarkNodeMoved) {
  shared_model_->CreateAccountPermanentFolders();

  ASSERT_THAT(local_view_.bookmark_bar_node(),
              Eq(shared_model_->bookmark_bar_node()));
  ASSERT_THAT(local_view_.mobile_node(), Eq(shared_model_->mobile_node()));
  ASSERT_THAT(local_view_.other_node(), Eq(shared_model_->other_node()));
  ASSERT_THAT(account_view_.bookmark_bar_node(),
              Eq(shared_model_->account_bookmark_bar_node()));
  ASSERT_THAT(account_view_.mobile_node(),
              Eq(shared_model_->account_mobile_node()));
  ASSERT_THAT(account_view_.other_node(),
              Eq(shared_model_->account_other_node()));

  const bookmarks::BookmarkNode* node1 = shared_model_->AddURL(
      shared_model_->bookmark_bar_node(), 0, kLocalTitle1, kLocalUrl1);
  const bookmarks::BookmarkNode* node2 = shared_model_->AddURL(
      shared_model_->account_bookmark_bar_node(), 0, kLocalTitle2, kLocalUrl2);

  // Move from local to local.
  {
    EXPECT_CALL(local_view_observer_,
                BookmarkNodeMoved(shared_model_->bookmark_bar_node(), 0,
                                  shared_model_->mobile_node(), 0));
    EXPECT_CALL(account_view_observer_, BookmarkNodeMoved).Times(0);
    shared_model_->Move(node1, shared_model_->mobile_node(), 0);
    testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
    testing::Mock::VerifyAndClearExpectations(&account_view_observer_);
  }

  // Move from account to account.
  {
    EXPECT_CALL(account_view_observer_,
                BookmarkNodeMoved(shared_model_->account_bookmark_bar_node(), 0,
                                  shared_model_->account_mobile_node(), 0));
    EXPECT_CALL(local_view_observer_, BookmarkNodeMoved).Times(0);
    shared_model_->Move(node2, shared_model_->account_mobile_node(), 0);
    testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
    testing::Mock::VerifyAndClearExpectations(&account_view_observer_);
  }

  // Move from local to account.
  {
    EXPECT_CALL(
        local_view_observer_,
        OnWillRemoveBookmarks(shared_model_->mobile_node(), 0, node1, _));
    EXPECT_CALL(
        local_view_observer_,
        BookmarkNodeRemoved(shared_model_->mobile_node(), 0, node1, _, _));
    EXPECT_CALL(account_view_observer_,
                BookmarkNodeAdded(shared_model_->account_bookmark_bar_node(), 0,
                                  false));
    EXPECT_CALL(local_view_observer_, BookmarkNodeAdded).Times(0);
    EXPECT_CALL(local_view_observer_, BookmarkNodeMoved).Times(0);
    EXPECT_CALL(account_view_observer_, OnWillRemoveBookmarks).Times(0);
    EXPECT_CALL(account_view_observer_, BookmarkNodeRemoved).Times(0);
    EXPECT_CALL(account_view_observer_, BookmarkNodeMoved).Times(0);
    shared_model_->Move(node1, shared_model_->account_bookmark_bar_node(), 0);
    testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
    testing::Mock::VerifyAndClearExpectations(&account_view_observer_);
  }

  // Move from account to local.
  {
    EXPECT_CALL(account_view_observer_,
                OnWillRemoveBookmarks(shared_model_->account_mobile_node(), 0,
                                      node2, _));
    EXPECT_CALL(account_view_observer_,
                BookmarkNodeRemoved(shared_model_->account_mobile_node(), 0,
                                    node2, _, _));
    EXPECT_CALL(
        local_view_observer_,
        BookmarkNodeAdded(shared_model_->bookmark_bar_node(), 0, false));
    EXPECT_CALL(account_view_observer_, BookmarkNodeAdded).Times(0);
    EXPECT_CALL(account_view_observer_, BookmarkNodeMoved).Times(0);
    EXPECT_CALL(local_view_observer_, OnWillRemoveBookmarks).Times(0);
    EXPECT_CALL(local_view_observer_, BookmarkNodeRemoved).Times(0);
    EXPECT_CALL(local_view_observer_, BookmarkNodeMoved).Times(0);
    shared_model_->Move(node2, shared_model_->bookmark_bar_node(), 0);
    testing::Mock::VerifyAndClearExpectations(&account_view_observer_);
    testing::Mock::VerifyAndClearExpectations(&local_view_observer_);
  }
}

TEST_F(LegacyBookmarkModelWithSharedUnderlyingModelTest, Destruction) {
  testing::NiceMock<bookmarks::MockBookmarkModelObserver> observer;

  {
    LegacyBookmarkModelWithSharedUnderlyingModel view(
        /*underlying_model=*/shared_model_.get(),
        bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes,
        /*managed_bookmark_service=*/nullptr);
    view.AddObserver(&observer);
    EXPECT_CALL(observer, BookmarkModelBeingDeleted()).WillOnce([&]() {
      view.RemoveObserver(&observer);
    });
  }

  // `view` goes out of scope here and that should have invoked
  // BookmarkModelBeingDeleted().
}

}  // namespace ios
