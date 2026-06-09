// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_presenter.h"

#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_view_controller.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface TestOmniboxPopupPresenterDelegate
    : NSObject <OmniboxPopupPresenterDelegate>
@property(nonatomic, strong) UIView* parentView;
@property(nonatomic, strong) UIViewController* parentViewController;
@property(nonatomic, assign) NSInteger popupParentViewCallCount;
@property(nonatomic, assign) BOOL popupDidOpenCalled;
@property(nonatomic, assign) BOOL popupDidCloseCalled;
@property(nonatomic, assign) BOOL popupDidInitializePresenterCalled;
@end

@implementation TestOmniboxPopupPresenterDelegate
- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter {
  self.popupParentViewCallCount++;
  return self.parentView;
}
- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter {
  return self.parentViewController;
}
- (UIColor*)popupBackgroundColorForPresenter:(OmniboxPopupPresenter*)presenter {
  return [UIColor whiteColor];
}
- (GuideName*)omniboxGuideNameForPresenter:(OmniboxPopupPresenter*)presenter {
  return nil;
}
- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter {
  self.popupDidOpenCalled = YES;
}
- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
  self.popupDidCloseCalled = YES;
}
- (void)popupDidInitializePresenter:(OmniboxPopupPresenter*)presenter {
  self.popupDidInitializePresenterCalled = YES;
}
@end

class OmniboxPopupPresenterTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    testDelegate_ = [[TestOmniboxPopupPresenterDelegate alloc] init];
    mockViewController_ = OCMClassMock([OmniboxPopupViewController class]);

    dummyView_ = [[UIView alloc] init];
    OCMStub([mockViewController_ view]).andReturn(dummyView_);

    testDelegate_.parentView = [[UIView alloc] init];
    testDelegate_.parentViewController = OCMClassMock([UIViewController class]);
  }

  void TearDown() override { PlatformTest::TearDown(); }

  TestOmniboxPopupPresenterDelegate* testDelegate_;
  id mockViewController_;
  UIView* dummyView_;
};

// Tests that updatePopupAfterTraitCollectionChange does nothing if the popup is
// not open.
TEST_F(OmniboxPopupPresenterTest, UpdateAfterTraitCollectionChangeWhenClosed) {
  OmniboxPopupPresenter* presenter = [[OmniboxPopupPresenter alloc]
      initWithPopupPresenterDelegate:testDelegate_
                 popupViewController:mockViewController_
                   layoutGuideCenter:nil
                           incognito:NO
                 presentationContext:OmniboxPresentationContext::kLocationBar];

  EXPECT_FALSE(presenter.isOpen);

  NSInteger initialCallCount = testDelegate_.popupParentViewCallCount;

  [presenter updatePopupAfterTraitCollectionChange];

  // Should not have called delegate to get parent view.
  EXPECT_EQ(testDelegate_.popupParentViewCallCount, initialCallCount);
}

// Tests that updatePopupAfterTraitCollectionChange re-adds the popup if it is
// open.
TEST_F(OmniboxPopupPresenterTest, UpdateAfterTraitCollectionChangeWhenOpen) {
  OmniboxPopupPresenter* presenter = [[OmniboxPopupPresenter alloc]
      initWithPopupPresenterDelegate:testDelegate_
                 popupViewController:mockViewController_
                   layoutGuideCenter:nil
                           incognito:NO
                 presentationContext:OmniboxPresentationContext::kLocationBar];

  // Setup for opening
  OCMStub([mockViewController_ hasContent]).andReturn(YES);

  // Call updatePopupOnFocus to open it
  [presenter updatePopupOnFocus:YES];

  EXPECT_TRUE(presenter.isOpen);
  EXPECT_TRUE(testDelegate_.popupDidOpenCalled);

  NSInteger callCountAfterOpen = testDelegate_.popupParentViewCallCount;
  EXPECT_GT(callCountAfterOpen, 0);

  [presenter updatePopupAfterTraitCollectionChange];

  // Should have called delegate again during trait change.
  EXPECT_GT(testDelegate_.popupParentViewCallCount, callCountAfterOpen);
}
