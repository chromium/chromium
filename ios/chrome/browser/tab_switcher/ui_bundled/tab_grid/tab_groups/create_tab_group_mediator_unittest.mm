// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_tab_group_mediator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_tab_group_mediator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_creation_consumer.h"
#import "testing/platform_test.h"

@interface TabGroupCreationConsumer : NSObject <TabGroupCreationConsumer>
@end

@implementation TabGroupCreationConsumer

- (void)setDefaultGroupColor:(tab_groups::TabGroupColorId)color {
}

- (void)setGroupTitle:(NSString*)title {
}

- (void)setSnapshotAndFavicon:(TabSnapshotAndFavicon*)tabSnapshotAndFavicon
                     tabIndex:(NSInteger)tabIndex {
}

- (void)setTabsCount:(NSInteger)tabsCount {
}

@end

@interface FakeCreateTabMediatorDelegate
    : NSObject <CreateTabGroupMediatorDelegate>
@property(nonatomic, assign) NSInteger editedGroupWasExternallyMutatedCallCount;
@property(nonatomic, weak) CreateTabGroupMediator* mediator;
@end

@implementation FakeCreateTabMediatorDelegate

#pragma mark CreateTabMediatorDelegate

- (void)createTabGroupMediatorEditedGroupWasExternallyMutated:
    (CreateTabGroupMediator*)mediator {
  _editedGroupWasExternallyMutatedCallCount++;
  _mediator = mediator;
}

@end

namespace {

// Fake WebStateList delegate that attaches the required snapshot  tab helper.
class FakeWebStateListDelegateWithSnapshotTabHelper
    : public FakeWebStateListDelegate {
 public:
  FakeWebStateListDelegateWithSnapshotTabHelper() {}
  ~FakeWebStateListDelegateWithSnapshotTabHelper() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    SnapshotTabHelper::CreateForWebState(web_state);
    SnapshotSourceTabHelper::CreateForWebState(web_state);
  }
};

class CreateTabGroupMediatorTest : public PlatformTest {
 protected:
  CreateTabGroupMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(),
        std::make_unique<FakeWebStateListDelegateWithSnapshotTabHelper>());
    builder_ = std::make_unique<WebStateListBuilderFromDescription>(
        browser_->GetWebStateList());
    EXPECT_TRUE(
        builder_->BuildWebStateListFromDescription("a | b [ 0 c ] [ 1 d ]"));
    consumer_ = [[TabGroupCreationConsumer alloc] init];
    delegate_ = [[FakeCreateTabMediatorDelegate alloc] init];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<WebStateListBuilderFromDescription> builder_;
  TabGroupCreationConsumer* consumer_;
  FakeCreateTabMediatorDelegate* delegate_;
};

// Tests that when a mediator is configured to edit a group, it notifies its
// delegate when that group is deleted.
TEST_F(CreateTabGroupMediatorTest, DeletingGroupNotifiesDelegate) {
  const TabGroup* tab_group = builder_->GetTabGroupForIdentifier('0');
  CreateTabGroupMediator* mediator = [[CreateTabGroupMediator alloc]
      initTabGroupEditionWithConsumer:consumer_
                             tabGroup:tab_group
                              browser:browser_.get()
                        faviconLoader:nullptr];
  mediator.delegate = delegate_;
  EXPECT_EQ(delegate_.editedGroupWasExternallyMutatedCallCount, 0);

  browser_->GetWebStateList()->DeleteGroup(tab_group);

  EXPECT_EQ(delegate_.editedGroupWasExternallyMutatedCallCount, 1);
  EXPECT_EQ(delegate_.mediator, mediator);
  [mediator disconnect];
}

// Tests that when a mediator is configured to edit a group, it doesn't notify
// its delegate when another group is deleted.
TEST_F(CreateTabGroupMediatorTest, DeletingOtherGroupDoesntNotifyDelegate) {
  const TabGroup* tab_group_0 = builder_->GetTabGroupForIdentifier('0');
  const TabGroup* tab_group_1 = builder_->GetTabGroupForIdentifier('1');
  CreateTabGroupMediator* mediator = [[CreateTabGroupMediator alloc]
      initTabGroupEditionWithConsumer:consumer_
                             tabGroup:tab_group_0
                              browser:browser_.get()
                        faviconLoader:nullptr];
  mediator.delegate = delegate_;
  EXPECT_EQ(delegate_.editedGroupWasExternallyMutatedCallCount, 0);

  browser_->GetWebStateList()->DeleteGroup(tab_group_1);

  EXPECT_EQ(delegate_.editedGroupWasExternallyMutatedCallCount, 0);
  EXPECT_EQ(delegate_.mediator, nil);
  [mediator disconnect];
}

// Tests that when a mediator is configured to edit a group, it notifies its
// delegate when that group's visual data is updated.
TEST_F(CreateTabGroupMediatorTest, UpdatingGroupNotifiesDelegate) {
  const TabGroup* tab_group = builder_->GetTabGroupForIdentifier('0');
  CreateTabGroupMediator* mediator = [[CreateTabGroupMediator alloc]
      initTabGroupEditionWithConsumer:consumer_
                             tabGroup:tab_group
                              browser:browser_.get()
                        faviconLoader:nullptr];
  mediator.delegate = delegate_;
  EXPECT_EQ(delegate_.editedGroupWasExternallyMutatedCallCount, 0);
  tab_groups::TabGroupVisualData visual_data(u"Updated Group Name",
                                             tab_groups::TabGroupColorId::kRed);

  browser_->GetWebStateList()->UpdateGroupVisualData(tab_group, visual_data);

  EXPECT_EQ(delegate_.editedGroupWasExternallyMutatedCallCount, 1);
  EXPECT_EQ(delegate_.mediator, mediator);
  [mediator disconnect];
}

// Tests that when a mediator is configured to edit a group, it doesn't notify
// its delegate when another group is updated.
TEST_F(CreateTabGroupMediatorTest, UpdatingOtherGroupDoesntNotifyDelegate) {
  const TabGroup* tab_group_0 = builder_->GetTabGroupForIdentifier('0');
  const TabGroup* tab_group_1 = builder_->GetTabGroupForIdentifier('1');
  CreateTabGroupMediator* mediator = [[CreateTabGroupMediator alloc]
      initTabGroupEditionWithConsumer:consumer_
                             tabGroup:tab_group_0
                              browser:browser_.get()
                        faviconLoader:nullptr];
  mediator.delegate = delegate_;
  EXPECT_EQ(delegate_.editedGroupWasExternallyMutatedCallCount, 0);
  tab_groups::TabGroupVisualData visual_data(u"Updated Group Name",
                                             tab_groups::TabGroupColorId::kRed);

  browser_->GetWebStateList()->UpdateGroupVisualData(tab_group_1, visual_data);

  EXPECT_EQ(delegate_.editedGroupWasExternallyMutatedCallCount, 0);
  EXPECT_EQ(delegate_.mediator, nil);
  [mediator disconnect];
}

}  // namespace
