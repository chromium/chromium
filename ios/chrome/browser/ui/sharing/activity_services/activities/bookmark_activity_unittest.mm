// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/bookmark_activity.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

NSString* const kTestTitle = @"Test Title";

}  // namespace

// Test fixture for covering the BookmarkActivity class.
class BookmarkActivityTest : public BookmarkIOSUnitTestSupport {
 protected:
  BookmarkActivityTest() {}

  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();

    mocked_handler_ = OCMProtocolMock(@protocol(BookmarksCommands));

    RegisterPrefs();
  }

  // Registers the edit bookmarks pref.
  void RegisterPrefs() {
    testing_pref_service_.registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled, true);
  }

  // Sets the edit bookmarks pref to `canEdit`.
  void SetCanEditBookmarkPref(bool canEdit) {
    testing_pref_service_.SetBoolean(bookmarks::prefs::kEditBookmarksEnabled,
                                     canEdit);
  }

  // Creates a BookmarkActivity instance with the given `URL`.
  BookmarkActivity* CreateActivity(const GURL& URL) {
    return [[BookmarkActivity alloc] initWithURL:URL
                                           title:kTestTitle
                                   bookmarkModel:bookmark_model_
                                         handler:mocked_handler_
                                     prefService:&testing_pref_service_];
  }

  TestingPrefServiceSimple testing_pref_service_;
  id mocked_handler_;
};

// Tests that the activity can only be performed if the preferences indicate
// that bookmarks can be edited.
TEST_F(BookmarkActivityTest, FlagOn_ActivityHiddenByPref) {
  BookmarkActivity* activity = CreateActivity(GURL());

  // Flag On, Editable bookmark pref true.
  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);

  SetCanEditBookmarkPref(false);

  // Flag On, Editable bookmark pref false.
  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that passing a nil bookmarkModel won't crash the activity.
TEST_F(BookmarkActivityTest, NilBookmarkModel_NoCrash) {
  BookmarkActivity* activity =
      [[BookmarkActivity alloc] initWithURL:GURL("https://example.com/")
                                      title:kTestTitle
                              bookmarkModel:nil
                                    handler:mocked_handler_
                                prefService:&testing_pref_service_];

  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the title of the activity is add when URL is not bookmarked.
TEST_F(BookmarkActivityTest, ActivityTitle_AddBookmark) {
  GURL testUrl("https://example.com/");
  BookmarkActivity* activity = CreateActivity(testUrl);

  NSString* addBookmarkString =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS);
  EXPECT_NSEQ(addBookmarkString, activity.activityTitle);
}

// Tests that the title of the activity is edit when URL is already bookmarked.
TEST_F(BookmarkActivityTest, ActivityTitle_EditBookmark) {
  // Add a bookmark.
  const bookmarks::BookmarkNode* bookmark =
      AddBookmark(bookmark_model_->mobile_node(), u"activity_test");
  ASSERT_TRUE(bookmark_model_->IsBookmarked(bookmark->url()));

  BookmarkActivity* activity = CreateActivity(bookmark->url());

  NSString* editBookmarkString =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK);
  EXPECT_NSEQ(editBookmarkString, activity.activityTitle);
}

TEST_F(BookmarkActivityTest, PerformActivity_BookmarkAddCommand) {
  GURL testUrl("https://example.com/");
  BookmarkActivity* activity = CreateActivity(testUrl);

  [[mocked_handler_ expect]
      createOrEditBookmarkWithURL:[OCMArg
                                      checkWithBlock:^BOOL(URLWithTitle* URL) {
                                        EXPECT_EQ(testUrl, URL.URL);
                                        EXPECT_EQ(kTestTitle, URL.title);
                                        return YES;
                                      }]];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}
