// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/coordinator/assistant_container_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake delegate recording detent changes reported by the container.
@interface FakeAssistantContainerDelegate
    : NSObject <AssistantContainerDelegate>
@property(nonatomic, assign) std::optional<AssistantContainerDetent> lastDetent;
@end

@implementation FakeAssistantContainerDelegate

- (void)assistantContainer:(AssistantContainerViewController*)container
           didChangeDetent:(AssistantContainerDetent)detent {
  self.lastDetent = detent;
}

@end

namespace {

constexpr CGRect kWindowFrame = {{0.0, 0.0}, {390.0, 844.0}};
constexpr CGFloat kBottomToolbarHeight = 50.0;
constexpr CGFloat kKeyboardHeight = 300.0;

// Simulates a bottom toolbar observer that reports its obscured inset range and
// current obscured inset proportional to `bottom_progress()`.
class FakeBottomToolbarFullscreenObserver
    : public FullscreenBrowserAgentObserver {
 public:
  void WillUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override {
    agent->AddObscuredInsetRange(UIRectEdgeBottom, 0.0, kBottomToolbarHeight);
  }

  void WillUpdateState(FullscreenBrowserAgent* agent) override {
    agent->AddObscuredInset(UIRectEdgeBottom,
                            kBottomToolbarHeight * agent->bottom_progress());
  }
};

class AssistantContainerCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kFullscreenRefactoring);

    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[SceneState alloc] init];
    [scene_state_ addAgent:[[LayoutGuideSceneAgent alloc] init]];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    FullscreenBrowserAgent::CreateForBrowser(browser_.get());
    fullscreen_agent_ = FullscreenBrowserAgent::FromBrowser(browser_.get());
    fullscreen_agent_->AddObserver(&toolbar_fullscreen_observer_);
    fullscreen_agent_->InvalidateInsetRange();
    fullscreen_mediator_ = [[FullscreenMediator alloc]
        initWithBrowserAgent:fullscreen_agent_
                webStateList:browser_->GetWebStateList()
          browserLayoutState:browser_->GetBrowserLayoutState()];

    window_ = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    window_.frame = kWindowFrame;
    base_view_controller_ = [[UIViewController alloc] init];
    base_view_controller_.view.frame = window_.bounds;
    base_view_controller_.traitOverrides.horizontalSizeClass =
        UIUserInterfaceSizeClassCompact;
    window_.rootViewController = base_view_controller_;
    [window_ makeKeyAndVisible];
    [window_ layoutIfNeeded];

    coordinator_ = [[AssistantContainerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    [coordinator_ start];
    [coordinator_
        setAssistantContainerDetents:{
                                         AssistantContainerDetent::kMinimized,
                                         AssistantContainerDetent::kMedium,
                                         AssistantContainerDetent::kLarge,
                                     }];
    delegate_ = [[FakeAssistantContainerDelegate alloc] init];
  }

  void TearDown() override {
    [coordinator_ stopAnimated:NO completion:nil];
    coordinator_ = nil;
    [fullscreen_mediator_ disconnect];
    fullscreen_mediator_ = nil;
    fullscreen_agent_->RemoveObserver(&toolbar_fullscreen_observer_);
    window_.rootViewController = nil;
    window_ = nil;
    base_view_controller_ = nil;
    delegate_ = nil;
    fullscreen_agent_ = nullptr;
    browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  // Returns the presented `AssistantContainerViewController` child.
  AssistantContainerViewController* GetPresentedContainerViewController() {
    return base::apple::ObjCCastStrict<AssistantContainerViewController>(
        base_view_controller_.childViewControllers.firstObject);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_ = nil;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FullscreenBrowserAgent> fullscreen_agent_ = nullptr;
  FakeBottomToolbarFullscreenObserver toolbar_fullscreen_observer_;
  FullscreenMediator* fullscreen_mediator_ = nil;
  UIWindow* window_ = nil;
  UIViewController* base_view_controller_ = nil;
  AssistantContainerCoordinator* coordinator_ = nil;
  FakeAssistantContainerDelegate* delegate_ = nil;
};

// Tests that keyboard obscured inset updates while `bottom_progress` is 0 do
// not minimize or translate a presented container offscreen.
TEST_F(AssistantContainerCoordinatorTest,
       KeyboardInsetUpdatesKeepContainerPresentedInFullscreen) {
  [fullscreen_mediator_
      enterFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                        animated:NO];
  ASSERT_EQ(0.0, fullscreen_agent_->bottom_progress());

  UIViewController* content_view_controller = [[UIViewController alloc] init];
  [coordinator_ showAssistantContainerWithContent:content_view_controller
                                         delegate:delegate_];
  [coordinator_
      animateAssistantContainerToDetent:AssistantContainerDetent::kMedium
                               duration:0
                                  curve:UIViewAnimationCurveEaseInOut];

  AssistantContainerViewController* container_view_controller =
      GetPresentedContainerViewController();
  ASSERT_TRUE(container_view_controller);
  ASSERT_TRUE(delegate_.lastDetent.has_value());
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastDetent.value());
  EXPECT_TRUE(CGAffineTransformIsIdentity(
      container_view_controller.assistantContainerView.transform));

  // Simulate keyboard appearance and dismissal updating the obscured inset
  // range without changing `bottom_progress`.
  fullscreen_agent_->SetKeyboardObscuredInset(kKeyboardHeight);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastDetent.value());
  EXPECT_TRUE(CGAffineTransformIsIdentity(
      container_view_controller.assistantContainerView.transform));

  fullscreen_agent_->SetKeyboardObscuredInset(0.0);
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastDetent.value());
  EXPECT_TRUE(CGAffineTransformIsIdentity(
      container_view_controller.assistantContainerView.transform));
}

// Tests that actual fullscreen progress changes still minimize and translate
// the container offscreen and restore it when exiting fullscreen.
TEST_F(AssistantContainerCoordinatorTest, FollowsFullscreenProgressChanges) {
  ASSERT_EQ(1.0, fullscreen_agent_->bottom_progress());

  UIViewController* content_view_controller = [[UIViewController alloc] init];
  [coordinator_ showAssistantContainerWithContent:content_view_controller
                                         delegate:delegate_];
  [coordinator_
      animateAssistantContainerToDetent:AssistantContainerDetent::kMedium
                               duration:0
                                  curve:UIViewAnimationCurveEaseInOut];

  AssistantContainerViewController* container_view_controller =
      GetPresentedContainerViewController();
  ASSERT_TRUE(container_view_controller);
  ASSERT_TRUE(delegate_.lastDetent.has_value());
  EXPECT_EQ(AssistantContainerDetent::kMedium, delegate_.lastDetent.value());
  EXPECT_TRUE(CGAffineTransformIsIdentity(
      container_view_controller.assistantContainerView.transform));

  // Entering fullscreen should minimize and translate the container offscreen.
  [fullscreen_mediator_
      enterFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                        animated:NO];
  EXPECT_EQ(AssistantContainerDetent::kMinimized, delegate_.lastDetent.value());
  EXPECT_GT(container_view_controller.assistantContainerView.transform.ty, 0.0);

  // Exiting fullscreen should restore the container's identity transform.
  [fullscreen_mediator_
      exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                       animated:NO];
  EXPECT_TRUE(CGAffineTransformIsIdentity(
      container_view_controller.assistantContainerView.transform));
}

}  // namespace
