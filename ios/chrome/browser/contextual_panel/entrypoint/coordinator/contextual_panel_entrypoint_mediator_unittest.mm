// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// A fake ContextualPanelEntrypointConsumer for use in tests.
@interface FakeEntrypointConsumer : NSObject <ContextualPanelEntrypointConsumer>

@property(nonatomic, assign) BOOL entrypointIsShown;

@property(nonatomic, assign) BOOL entrypointIsLarge;

@property(nonatomic, assign) BOOL contextualPanelIsOpen;

@property(nonatomic, assign) base::WeakPtr<ContextualPanelItemConfiguration>
    currentConfiguration;

@end

@implementation FakeEntrypointConsumer

- (void)setEntrypointConfig:
    (base::WeakPtr<ContextualPanelItemConfiguration>)config {
  self.currentConfiguration = config;
}

- (void)hideEntrypoint {
  self.entrypointIsShown = NO;
}

- (void)showEntrypoint {
  self.entrypointIsShown = YES;
}

- (void)transitionToLargeEntrypoint {
  self.entrypointIsLarge = YES;
}

- (void)transitionToSmallEntrypoint {
  self.entrypointIsLarge = NO;
}

- (void)transitionToContextualPanelOpenedState:(BOOL)opened {
  self.contextualPanelIsOpen = opened;
}

@end

// Fake test implementation of ContextualPanelEntrypointMediatorDelegate
@interface FakeContextualPanelEntrypointMediatorDelegate
    : NSObject <ContextualPanelEntrypointMediatorDelegate>

@property(nonatomic, assign) BOOL canShowLargeContextualPanelEntrypoint;

@end

@implementation FakeContextualPanelEntrypointMediatorDelegate

- (BOOL)canShowLargeContextualPanelEntrypoint:
    (ContextualPanelEntrypointMediator*)mediator {
  return self.canShowLargeContextualPanelEntrypoint;
}

- (void)setLocationBarLabelCenteredBetweenContent:
            (ContextualPanelEntrypointMediator*)mediator
                                         centered:(BOOL)centered {
  // No-op.
}

- (void)disableFullscreen {
  // No-op.
}

- (void)enableFullscreen {
  // No-op.
}

@end

// Test fake to allow easier triggering of ContextualPanelTabHelperObserver
// methods.
class FakeContextualPanelTabHelper : public ContextualPanelTabHelper {
 public:
  explicit FakeContextualPanelTabHelper(
      web::WebState* web_state,
      std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models)
      : ContextualPanelTabHelper(web_state, models) {}

  static void CreateForWebState(
      web::WebState* web_state,
      std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models) {
    web_state->SetUserData(
        UserDataKey(),
        std::make_unique<FakeContextualPanelTabHelper>(web_state, models));
  }

  void AddObserver(ContextualPanelTabHelperObserver* observer) override {
    ContextualPanelTabHelper::AddObserver(observer);
    observers_.AddObserver(observer);
  }
  void RemoveObserver(ContextualPanelTabHelperObserver* observer) override {
    ContextualPanelTabHelper::RemoveObserver(observer);
    observers_.RemoveObserver(observer);
  }

  void CallContextualPanelTabHelperDestroyed() {
    for (auto& observer : observers_) {
      observer.ContextualPanelTabHelperDestroyed(this);
    }
  }

  void CallContextualPanelHasNewData(
      std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
          item_configurations) {
    for (auto& observer : observers_) {
      observer.ContextualPanelHasNewData(this, item_configurations);
    }
  }

  base::ObserverList<ContextualPanelTabHelperObserver, true> observers_;
};

class ContextualPanelEntrypointMediatorTest : public PlatformTest {
 public:
  ContextualPanelEntrypointMediatorTest()
      : web_state_list_(&web_state_list_delegate_) {
    auto web_state = std::make_unique<web::FakeWebState>();
    std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models;
    FakeContextualPanelTabHelper::CreateForWebState(web_state.get(), models);
    web_state_list_.InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));

    mediator_ = [[ContextualPanelEntrypointMediator alloc]
        initWithWebStateList:&web_state_list_];

    entrypoint_consumer_ = [[FakeEntrypointConsumer alloc] init];
    mediator_.consumer = entrypoint_consumer_;

    delegate_ = [[FakeContextualPanelEntrypointMediatorDelegate alloc] init];
    mediator_.delegate = delegate_;
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  ContextualPanelEntrypointMediator* mediator_;
  FakeEntrypointConsumer* entrypoint_consumer_;
  FakeContextualPanelEntrypointMediatorDelegate* delegate_;
};

// Tests that tapping the entrypoint opens and then closes the panel.
TEST_F(ContextualPanelEntrypointMediatorTest, TestEntrypointTapped) {
  [mediator_ entrypointTapped];
  EXPECT_TRUE(entrypoint_consumer_.contextualPanelIsOpen);

  [mediator_ entrypointTapped];
  EXPECT_FALSE(entrypoint_consumer_.contextualPanelIsOpen);
}

TEST_F(ContextualPanelEntrypointMediatorTest, TestTabHelperDestroyed) {
  // Start off with entrypoint showing.
  [entrypoint_consumer_ showEntrypoint];

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_.GetActiveWebState()));
  tab_helper->CallContextualPanelTabHelperDestroyed();

  EXPECT_FALSE(entrypoint_consumer_.entrypointIsShown);
}

// Tests that if one configuration is provided, the entrypoint becomes shown.
TEST_F(ContextualPanelEntrypointMediatorTest, TestOneConfiguration) {
  ContextualPanelItemConfiguration configuration(
      ContextualPanelItemType::SamplePanelItem);

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_.GetActiveWebState()));

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      item_configurations;
  item_configurations.push_back(configuration.weak_ptr_factory.GetWeakPtr());

  tab_helper->CallContextualPanelHasNewData(item_configurations);

  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);

  ASSERT_TRUE(entrypoint_consumer_.currentConfiguration);
  EXPECT_EQ(&configuration, entrypoint_consumer_.currentConfiguration.get());
}

// Tests that -disconnect doesn't crash and that nothing is observing the tab
// helper after disconnecting.s
TEST_F(ContextualPanelEntrypointMediatorTest, TestDisconnect) {
  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_.GetActiveWebState()));

  EXPECT_FALSE(tab_helper->observers_.empty());
  [mediator_ disconnect];
  EXPECT_TRUE(tab_helper->observers_.empty());
}

TEST_F(ContextualPanelEntrypointMediatorTest, TestLargeEntrypointAppears) {
  ContextualPanelItemConfiguration configuration(
      ContextualPanelItemType::SamplePanelItem);
  configuration.relevance = ContextualPanelItemConfiguration::high_relevance;
  configuration.entrypoint_message = "test";

  delegate_.canShowLargeContextualPanelEntrypoint = YES;

  FakeContextualPanelTabHelper* tab_helper =
      static_cast<FakeContextualPanelTabHelper*>(
          FakeContextualPanelTabHelper::FromWebState(
              web_state_list_.GetActiveWebState()));

  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      item_configurations;
  item_configurations.push_back(configuration.weak_ptr_factory.GetWeakPtr());

  tab_helper->CallContextualPanelHasNewData(item_configurations);

  // At first, the small entrypoint should be displayed.
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);

  // Advance time so that the large entrypoint is displayed.
  task_environment_.FastForwardBy(
      base::Seconds(LargeContextualPanelEntrypointDelayInSeconds()));
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsLarge);

  // Advance time until the large entrypoint transitions back to small.
  task_environment_.FastForwardBy(
      base::Seconds(LargeContextualPanelEntrypointDisplayedInSeconds()));
  EXPECT_TRUE(entrypoint_consumer_.entrypointIsShown);
  EXPECT_FALSE(entrypoint_consumer_.entrypointIsLarge);
}
