// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/menu/action_factory+protected.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#import "url/gurl.h"

namespace {
const MenuScenarioHistogram kTestMenuScenario =
    kMenuScenarioHistogramHistoryEntry;
}  // namespace

// Test fixture for the ActionFactory.
class ActionFactoryTest : public PlatformTest {
 protected:
  ActionFactoryTest() : test_title_(@"SomeTitle") {}

  // Creates a blue square.
  UIImage* CreateMockImage() {
    return ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(10, 10), [UIColor blueColor]);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  NSString* test_title_;
};

// Tests the creation of an action using the parameterized method, and verifies
// that the action has the right title and image.
TEST_F(ActionFactoryTest, CreateActionWithParameters) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* mockImage = CreateMockImage();

  UIAction* action = [factory actionWithTitle:test_title_
                                        image:mockImage
                                         type:MenuActionType::CopyURL
                                        block:^{
                                        }];

  EXPECT_TRUE([test_title_ isEqualToString:action.title]);
  EXPECT_EQ(mockImage, action.image);
}

// Tests that the bookmark action has the right title and image.
TEST_F(ActionFactoryTest, BookmarkAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_ADDTOBOOKMARKS);

  UIAction* action = [factory actionToBookmarkWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the close regular tab action has the right title and image.
TEST_F(ActionFactoryTest, CloseRegularTabAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_CLOSETAB);

  UIAction* action = [factory actionToCloseRegularTabWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the close pinned tab action has the right title and image.
TEST_F(ActionFactoryTest, ClosePinnedTabAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_CLOSEPINNEDTAB);

  UIAction* action = [factory actionToClosePinnedTabWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the copy action has the right title and image.
TEST_F(ActionFactoryTest, CopyAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];
  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kLinkActionSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_COPY_LINK_ACTION_TITLE);

  CrURL* testURL = [[CrURL alloc] initWithGURL:GURL("https://example.com")];
  UIAction* action = [factory actionToCopyURL:testURL];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the share action has the right title and image.
TEST_F(ActionFactoryTest, ShareAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kShareSymbol, kSymbolActionPointSize);
  NSString* expectedTitle = l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL);

  UIAction* action = [factory actionToShareWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the delete action has the right title and image.
TEST_F(ActionFactoryTest, DeleteAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kDeleteActionSymbol, kSymbolActionPointSize);
  NSString* expectedTitle = l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE);

  UIAction* action = [factory actionToDeleteWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
  EXPECT_EQ(UIMenuElementAttributesDestructive, action.attributes);
}

// Tests that the read later action has the right title and image.
TEST_F(ActionFactoryTest, ReadLaterAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kReadLaterActionSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST);

  UIAction* action = [factory actionToAddToReadingListWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the remove action has the right title and image.
TEST_F(ActionFactoryTest, RemoveAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolActionPointSize);
  NSString* expectedTitle = l10n_util::GetNSString(IDS_IOS_REMOVE_ACTION_TITLE);

  UIAction* action = [factory actionToRemoveWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the edit action has the right title and image.
TEST_F(ActionFactoryTest, EditAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kEditActionSymbol, kSymbolActionPointSize);
  NSString* expectedTitle = l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE);

  UIAction* action = [factory actionToEditWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the Open All Tabs action has the right title and image.
TEST_F(ActionFactoryTest, openAllTabsAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kPlusSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPEN_ALL_LINKS);

  UIAction* action = [factory actionToOpenAllTabsWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the hide action has the right title and image.
TEST_F(ActionFactoryTest, hideAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_RECENT_TABS_HIDE_MENU_OPTION);

  UIAction* action = [factory actionToHideWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the Move Folder action has the right title and image.
TEST_F(ActionFactoryTest, MoveFolderAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kMoveFolderSymbol, kSymbolActionPointSize));

  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE);

  UIAction* action = [factory actionToMoveFolderWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_NSEQ(expectedImage, action.image);
}

// Tests that the Mark As Read action has the right title and image.
TEST_F(ActionFactoryTest, markAsReadAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kMarkAsReadActionSymbol,
                                                      kSymbolActionPointSize);

  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_MARK_AS_READ_ACTION);

  UIAction* action = [factory actionToMarkAsReadWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the Mark As Unread action has the right title and image.
TEST_F(ActionFactoryTest, markAsUnreadAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kMarkAsUnreadActionSymbol,
                                                      kSymbolActionPointSize);

  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_MARK_AS_UNREAD_ACTION);

  UIAction* action = [factory actionToMarkAsUnreadWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the View Offline Version in New Tab action has the right title and
// image.
TEST_F(ActionFactoryTest, viewOfflineVersion) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kCheckmarkCircleSymbol,
                                                      kSymbolActionPointSize);

  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_OPEN_OFFLINE_BUTTON);

  UIAction* action = [factory actionToOpenOfflineVersionInNewTabWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the save image action has the right title and image.
TEST_F(ActionFactoryTest, SaveImageAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kSaveImageActionSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_SAVEIMAGE);

  UIAction* action = [factory actionSaveImageWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the copy image action has the right title and image.
TEST_F(ActionFactoryTest, CopyImageAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kCopyActionSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPYIMAGE);

  UIAction* action = [factory actionCopyImageWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the close all action has the right title and image.
TEST_F(ActionFactoryTest, CloseAllTabsAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABS);

  UIAction* action = [factory actionToCloseAllTabsWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the select tabs action has the right title and image.
TEST_F(ActionFactoryTest, SelectTabsAction) {
  ActionFactory* factory =
      [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kCheckmarkCircleSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_SELECTTABS);

  UIAction* action = [factory actionToSelectTabsWithBlock:^{
  }];

  EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
  EXPECT_EQ(expectedImage, action.image);
}
