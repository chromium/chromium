// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_follow_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channel_item.h"
#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channels_data_source.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// An example URL of a website to follow.
NSString* const kExampleURL = @"https://example.org/";
NSString* const kExampleTitle = @"Example Title";

// Test fixture for FollowManagementViewController testing.
class FollowManagementViewControllerTest : public PlatformTest {
 protected:
  FollowManagementViewControllerTest() {
    view_controller_ = [[FollowManagementViewController alloc]
        initWithStyle:UITableViewStyleInsetGrouped];
  }

  void AddViewControllerToWindow(UIViewController* view_controller) {
    [key_window_.Get() setRootViewController:view_controller];
  }

  void SetupMockDataSource() {
    mock_data_source_ =
        OCMProtocolMock(@protocol(FollowedWebChannelsDataSource));
    view_controller_.followedWebChannelsDataSource = mock_data_source_;
    NSArray* channels = @[ ExampleWebChannel() ];
    OCMStub([mock_data_source_ followedWebChannels]).andReturn(channels);
  }

  // Returns a FollowedWebChannel object with an example URL.
  FollowedWebChannel* ExampleWebChannel() {
    FollowedWebChannel* channel = [[FollowedWebChannel alloc] init];
    channel.title = kExampleTitle;
    channel.webPageURL =
        [[CrURL alloc] initWithNSURL:[NSURL URLWithString:kExampleURL]];
    channel.rssURL = channel.webPageURL;
    channel.faviconURL = channel.webPageURL;
    return channel;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedKeyWindow key_window_;
  id mock_data_source_;
  FollowManagementViewController* view_controller_;
};

// Tests that the view loads, and that it asks the data source to load.
TEST_F(FollowManagementViewControllerTest, ViewControllerLoads) {
  SetupMockDataSource();
  OCMExpect([mock_data_source_ loadFollowedWebSites]);

  AddViewControllerToWindow(view_controller_);
  EXPECT_TRUE(view_controller_.viewLoaded);
  EXPECT_OCMOCK_VERIFY(mock_data_source_);
}

// Tests that `updateFollowedWebSites` loads from the data source.
TEST_F(FollowManagementViewControllerTest, UpdateFollowedWebSites) {
  SetupMockDataSource();
  OCMExpect([mock_data_source_ loadFollowedWebSites]);

  AddViewControllerToWindow(view_controller_);
  UITableView* table_view = view_controller_.tableView;
  EXPECT_EQ([table_view numberOfRowsInSection:0], 0);
  [view_controller_ updateFollowedWebSites];
  EXPECT_EQ([table_view numberOfRowsInSection:0], 1);
  EXPECT_OCMOCK_VERIFY(mock_data_source_);
}

// Tests that followed web channels are added to the table view.
TEST_F(FollowManagementViewControllerTest, AddsFollowedWebChannel) {
  AddViewControllerToWindow(view_controller_);

  UITableView* table_view = view_controller_.tableView;
  EXPECT_EQ([table_view numberOfRowsInSection:0], 0);
  [view_controller_ addFollowedWebChannel:ExampleWebChannel()];
  EXPECT_EQ([table_view numberOfRowsInSection:0], 1);

  NSIndexPath* first_row = [NSIndexPath indexPathForRow:0 inSection:0];
  FollowedWebChannelCell* cell =
      base::apple::ObjCCastStrict<FollowedWebChannelCell>(
          [table_view cellForRowAtIndexPath:first_row]);
  EXPECT_NSEQ(cell.titleLabel.text, kExampleTitle);
}

// Tests that unfollowed web channels are removed from the table view.
TEST_F(FollowManagementViewControllerTest, RemovesUnfollowedWebChannel) {
  AddViewControllerToWindow(view_controller_);
  UITableView* table_view = view_controller_.tableView;
  EXPECT_EQ([table_view numberOfRowsInSection:0], 0);
  [view_controller_ addFollowedWebChannel:ExampleWebChannel()];

  EXPECT_EQ([table_view numberOfRowsInSection:0], 1);
  [view_controller_ removeFollowedWebChannel:ExampleWebChannel()];
  EXPECT_EQ([table_view numberOfRowsInSection:0], 0);
}

// Tests that a menu is presented when the user selects a row.
TEST_F(FollowManagementViewControllerTest, DidSelectRow) {
  if (@available(iOS 16.0, *)) {
    scoped_feature_list_.InitWithFeatures({kEnableUIEditMenuInteraction}, {});
    SetupMockDataSource();

    id mock_interaction_menu = OCMClassMock([UIEditMenuInteraction class]);
    OCMStub([mock_interaction_menu alloc]).andReturn(mock_interaction_menu);
    OCMStub([mock_interaction_menu initWithDelegate:(id)(view_controller_)])
        .andReturn(mock_interaction_menu);

    AddViewControllerToWindow(view_controller_);
    UITableView* table_view = view_controller_.tableView;
    EXPECT_EQ([table_view numberOfRowsInSection:0], 0);
    [view_controller_ updateFollowedWebSites];
    EXPECT_OCMOCK_VERIFY(mock_data_source_);

    OCMExpect(
        [mock_interaction_menu presentEditMenuWithConfiguration:[OCMArg any]]);
    NSIndexPath* first_row = [NSIndexPath indexPathForRow:0 inSection:0];
    [view_controller_ tableView:table_view didSelectRowAtIndexPath:first_row];
    EXPECT_OCMOCK_VERIFY(mock_interaction_menu);
  }
}

// Tests that the unfollow swipe action is returned.
TEST_F(FollowManagementViewControllerTest, UnfollowSwipeAction) {
  SetupMockDataSource();
  AddViewControllerToWindow(view_controller_);
  UITableView* table_view = view_controller_.tableView;
  EXPECT_EQ([table_view numberOfRowsInSection:0], 0);
  [view_controller_ updateFollowedWebSites];

  NSIndexPath* first_row = [NSIndexPath indexPathForRow:0 inSection:0];
  UISwipeActionsConfiguration* config = [view_controller_ tableView:table_view
                 trailingSwipeActionsConfigurationForRowAtIndexPath:first_row];

  EXPECT_NE(nil, config);
  EXPECT_EQ(config.actions.count, 1u);
  EXPECT_NSEQ(
      config.actions[0].title,
      l10n_util::GetNSString(IDS_IOS_FOLLOW_MANAGEMENT_UNFOLLOW_ACTION));

  id mock_follow_delegate =
      OCMProtocolMock(@protocol(FollowManagementFollowDelegate));
  OCMExpect([mock_follow_delegate unfollowFollowedWebChannel:[OCMArg any]]);
  view_controller_.followDelegate = mock_follow_delegate;

  __block BOOL done = NO;
  UIContextualActionHandler handler = config.actions[0].handler;
  handler(config.actions[0], table_view, ^(BOOL actionPerformed) {
    done = actionPerformed;
  });
  EXPECT_EQ(done, YES);
  EXPECT_OCMOCK_VERIFY(mock_follow_delegate);
}

// Tests that a context menu configuration is returned.
TEST_F(FollowManagementViewControllerTest, ContextMenuConfig) {
  SetupMockDataSource();
  AddViewControllerToWindow(view_controller_);
  UITableView* table_view = view_controller_.tableView;
  EXPECT_EQ([table_view numberOfRowsInSection:0], 0);
  [view_controller_ updateFollowedWebSites];

  NSIndexPath* first_row = [NSIndexPath indexPathForRow:0 inSection:0];
  UIContextMenuConfiguration* config =
      [view_controller_ tableView:table_view
          contextMenuConfigurationForRowAtIndexPath:first_row
                                              point:CGPointMake(0, 0)];
  EXPECT_NE(nil, config);
}

}  // namespace
