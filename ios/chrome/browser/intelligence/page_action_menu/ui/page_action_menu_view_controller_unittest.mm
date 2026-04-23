// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_content_entry_point.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_mutator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_metrics.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

// Category to expose private methods for testing.
// This allows the test to directly call action handlers that are normally
// triggered by UI events (taps).
@interface PageActionMenuViewController (Testing)
- (void)handleGeminiTapped:(UIButton*)button;
- (void)handleLensEntryPointTapped:(UIButton*)button;
- (void)handleReaderModeTapped:(UIButton*)button;
- (void)handleReaderModeOptionsTapped:(UIButton*)button;
- (void)dismissPageActionMenu;
- (void)updateFooterContent;
@end

namespace {
// Performs a depth-first traversal of the view hierarchy to find a view with
// the given accessibility identifier.
UIView* FindViewByAccessibilityIdentifier(UIView* view, NSString* identifier) {
  if (!identifier) {
    return nil;
  }
  if ([view.accessibilityIdentifier isEqualToString:identifier]) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* found = FindViewByAccessibilityIdentifier(subview, identifier);
    if (found) {
      return found;
    }
  }
  return nil;
}
}  // namespace

// Test fixture for PageActionMenuViewController.
class PageActionMenuViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // Set up a test profile and browser.
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Create mocks for all dependencies injected into the view controller.
    mock_mutator_ = OCMProtocolMock(@protocol(PageActionMenuMutator));
    mock_delegate_ =
        OCMProtocolMock(@protocol(PageActionMenuViewControllerDelegate));
    mock_bwg_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    mock_page_action_menu_handler_ =
        OCMProtocolMock(@protocol(PageActionMenuCommands));
    mock_lens_overlay_handler_ =
        OCMProtocolMock(@protocol(LensOverlayCommands));
    mock_reader_mode_handler_ = OCMProtocolMock(@protocol(ReaderModeCommands));

    // Initialize the view controller and inject mocks.
    view_controller_ = [[PageActionMenuViewController alloc] init];
    view_controller_.mutator = mock_mutator_;
    view_controller_.delegate = mock_delegate_;
    view_controller_.BWGHandler = mock_bwg_handler_;
    view_controller_.pageActionMenuHandler = mock_page_action_menu_handler_;
    view_controller_.lensOverlayHandler = mock_lens_overlay_handler_;
    view_controller_.readerModeHandler = mock_reader_mode_handler_;

    // Stub mutator methods with defaults to avoid fragile tests.
    StubMutatorWithDefaults();
  }

  void TearDown() override {
    view_controller_ = nil;
    mock_mutator_ = nil;
    mock_delegate_ = nil;
    mock_bwg_handler_ = nil;
    mock_page_action_menu_handler_ = nil;
    mock_lens_overlay_handler_ = nil;
    mock_reader_mode_handler_ = nil;
    browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  void StubMutatorWithDefaults() {
    OCMStub([mock_mutator_ isReaderModeAvailable]).andReturn(YES);
    OCMStub([mock_mutator_ shouldShowFeatureEntryPoints]).andReturn(YES);

    PageActionMenuContentEntryPoint* enabledEntryPoint =
        [[PageActionMenuContentEntryPoint alloc] initWithEnabled:YES];
    OCMStub([mock_mutator_ lensEntryPointForTraitCollection:[OCMArg any]])
        .andReturn(enabledEntryPoint);
    OCMStub([mock_mutator_ readerModeEntryPoint]).andReturn(enabledEntryPoint);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  PageActionMenuViewController* view_controller_;

  id mock_mutator_;
  id mock_delegate_;
  id mock_bwg_handler_;
  id mock_page_action_menu_handler_;
  id mock_lens_overlay_handler_;
  id mock_reader_mode_handler_;
};

// Tests that the view controller correctly stores its injected dependencies.
TEST_F(PageActionMenuViewControllerTest, Initialization) {
  EXPECT_NE(view_controller_, nil);
  EXPECT_EQ(view_controller_.mutator, mock_mutator_);
  EXPECT_EQ(view_controller_.delegate, mock_delegate_);
  EXPECT_EQ(view_controller_.BWGHandler, mock_bwg_handler_);
  EXPECT_EQ(view_controller_.pageActionMenuHandler,
            mock_page_action_menu_handler_);
  EXPECT_EQ(view_controller_.lensOverlayHandler, mock_lens_overlay_handler_);
  EXPECT_EQ(view_controller_.readerModeHandler, mock_reader_mode_handler_);
}

// Tests that the view loads successfully.
TEST_F(PageActionMenuViewControllerTest, ViewDidLoad) {
  [view_controller_ loadViewIfNeeded];
  EXPECT_NE(view_controller_.view, nil);
}

// Tests that the Gemini button is enabled and fully opaque when Gemini is
// available for the current page.
TEST_F(PageActionMenuViewControllerTest,
       PageLoadStatusChanged_GeminiAvailable) {
  PageActionMenuContentEntryPoint* entryPoint =
      [[PageActionMenuContentEntryPoint alloc] initWithEnabled:YES];
  OCMStub([mock_mutator_ geminiEntryPoint]).andReturn(entryPoint);
  OCMStub([mock_mutator_ shouldShowFeatureEntryPoints]).andReturn(YES);

  [view_controller_ loadViewIfNeeded];

  // Access the private button via accessibility identifier.
  UIButton* bwgButton = (UIButton*)FindViewByAccessibilityIdentifier(
      view_controller_.view, kAIHubAskGeminiButtonAccessibilityIdentifier);
  EXPECT_NE(bwgButton, nil);

  [view_controller_ pageLoadStatusChanged];

  EXPECT_TRUE(bwgButton.enabled);
  EXPECT_TRUE(bwgButton.userInteractionEnabled);
  EXPECT_NEAR(bwgButton.alpha, 1.0, 0.001);
}

// Tests that the Gemini button is disabled and semi-transparent when Gemini is
// not available for the current page.
TEST_F(PageActionMenuViewControllerTest,
       PageLoadStatusChanged_GeminiNotAvailable) {
  PageActionMenuContentEntryPoint* entryPoint =
      [[PageActionMenuContentEntryPoint alloc] initWithEnabled:NO];
  OCMStub([mock_mutator_ geminiEntryPoint]).andReturn(entryPoint);
  OCMStub([mock_mutator_ shouldShowFeatureEntryPoints]).andReturn(YES);

  [view_controller_ loadViewIfNeeded];

  UIButton* bwgButton = (UIButton*)FindViewByAccessibilityIdentifier(
      view_controller_.view, kAIHubAskGeminiButtonAccessibilityIdentifier);
  EXPECT_NE(bwgButton, nil);

  [view_controller_ pageLoadStatusChanged];

  EXPECT_FALSE(bwgButton.enabled);
  EXPECT_FALSE(bwgButton.userInteractionEnabled);
  EXPECT_NEAR(bwgButton.alpha, 0.5, 0.001);
}

// Tests that setting the selected font family updates the Reader Mode options
// button subtitle with the correct localized string.
TEST_F(PageActionMenuViewControllerTest, SetSelectedFontFamily) {
  OCMStub([mock_mutator_ isReaderModeActive]).andReturn(YES);
  [view_controller_ loadViewIfNeeded];

  [view_controller_
      setSelectedFontFamily:dom_distiller::mojom::FontFamily::kSerif];
  NSString* expectedSubstring = l10n_util::GetNSString(
      IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SERIF_LABEL);

  UILabel* subtitleLabel =
      [view_controller_ valueForKey:@"readerModeOptionsButtonSubtitleLabel"];
  EXPECT_TRUE([subtitleLabel.text containsString:expectedSubstring]);

  [view_controller_
      setSelectedFontFamily:dom_distiller::mojom::FontFamily::kSansSerif];
  expectedSubstring = l10n_util::GetNSString(
      IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SANS_SERIF_LABEL);
  EXPECT_TRUE([subtitleLabel.text containsString:expectedSubstring]);
  EXPECT_NE(subtitleLabel.superview, nil);
}

// Tests that tapping the dismiss button triggers the command to dismiss the
// page action menu.
TEST_F(PageActionMenuViewControllerTest, DismissButtonTapped) {
  [view_controller_ loadViewIfNeeded];
  OCMExpect(
      [mock_page_action_menu_handler_ dismissPageActionMenuWithCompletion:nil]);
  [view_controller_ dismissPageActionMenu];

  // `self` is needed by OCMVerifyAll macro in C++ tests.
  id self = nil;
  OCMVerifyAll(mock_page_action_menu_handler_);
}

// Tests that tapping the Reader Mode options button notifies the delegate.
TEST_F(PageActionMenuViewControllerTest, ReaderModeOptionsButtonTapped) {
  OCMStub([mock_mutator_ isReaderModeActive]).andReturn(YES);
  [view_controller_ loadViewIfNeeded];

  OCMExpect([mock_delegate_
      viewControllerDidTapReaderModeOptionsButton:view_controller_]);
  [view_controller_ handleReaderModeOptionsTapped:nil];

  // `self` is needed by OCMVerifyAll macro in C++ tests.
  id self = nil;
  OCMVerifyAll(mock_delegate_);
}

// Tests that tapping the button to hide Reader Mode dismisses the menu and
// calls the Reader Mode handler to hide it.
TEST_F(PageActionMenuViewControllerTest, HideReaderModeButtonTapped) {
  OCMStub([mock_mutator_ isReaderModeActive]).andReturn(YES);
  OCMStub([mock_mutator_ shouldShowFeatureEntryPoints]).andReturn(NO);
  [view_controller_ loadViewIfNeeded];

  // Expect the menu to be dismissed first, and then call the completion block.
  OCMExpect([mock_page_action_menu_handler_
                dismissPageActionMenuWithCompletion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion)(void);
        [invocation getArgument:&completion atIndex:2];
        if (completion) {
          completion();
        }
      });
  OCMExpect([mock_reader_mode_handler_ hideReaderMode]);

  [view_controller_ handleReaderModeTapped:nil];

  // `self` is needed by OCMVerifyAll macro in C++ tests.
  id self = nil;
  OCMVerifyAll(mock_page_action_menu_handler_);
  OCMVerifyAll(mock_reader_mode_handler_);
}

// Tests that tapping the button to show Reader Mode dismisses the menu and
// calls the Reader Mode handler to show it from the AI Hub access point.
TEST_F(PageActionMenuViewControllerTest, ShowReaderModeButtonTapped) {
  OCMStub([mock_mutator_ isReaderModeActive]).andReturn(NO);
  PageActionMenuContentEntryPoint* entryPoint =
      [[PageActionMenuContentEntryPoint alloc] initWithEnabled:YES];
  OCMStub([mock_mutator_ readerModeEntryPoint]).andReturn(entryPoint);
  OCMStub([mock_mutator_ shouldShowFeatureEntryPoints]).andReturn(YES);
  [view_controller_ loadViewIfNeeded];

  OCMExpect([mock_page_action_menu_handler_
                dismissPageActionMenuWithCompletion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion)(void);
        [invocation getArgument:&completion atIndex:2];
        if (completion) {
          completion();
        }
      });
  OCMExpect([mock_reader_mode_handler_
      showReaderModeFromAccessPoint:ReaderModeAccessPoint::kAIHub]);

  [view_controller_ handleReaderModeTapped:nil];

  // `self` is needed by OCMVerifyAll macro in C++ tests.
  id self = nil;
  OCMVerifyAll(mock_page_action_menu_handler_);
  OCMVerifyAll(mock_reader_mode_handler_);
}

// Tests that tapping the Lens button dismisses the menu and calls the Lens
// handler to create and show the Lens UI.
TEST_F(PageActionMenuViewControllerTest, LensButtonTapped) {
  PageActionMenuContentEntryPoint* entryPoint =
      [[PageActionMenuContentEntryPoint alloc] initWithEnabled:YES];
  OCMStub([mock_mutator_ lensEntryPointForTraitCollection:[OCMArg any]])
      .andReturn(entryPoint);
  OCMStub([mock_mutator_ shouldShowFeatureEntryPoints]).andReturn(YES);
  [view_controller_ loadViewIfNeeded];

  OCMExpect([mock_page_action_menu_handler_
                dismissPageActionMenuWithCompletion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion)(void);
        [invocation getArgument:&completion atIndex:2];
        if (completion) {
          completion();
        }
      });
  OCMExpect([mock_lens_overlay_handler_
      createAndShowLensUI:YES
               entrypoint:LensOverlayEntrypoint::kAIHub
               completion:nil]);

  UIButton* lensButton = (UIButton*)FindViewByAccessibilityIdentifier(
      view_controller_.view, kAIHubLensButtonAccessibilityIdentifier);
  [view_controller_ handleLensEntryPointTapped:lensButton];

  // `self` is needed by OCMVerifyAll macro in C++ tests.
  id self = nil;
  OCMVerifyAll(mock_page_action_menu_handler_);
  OCMVerifyAll(mock_lens_overlay_handler_);
}

// Tests that tapping the Gemini button dismisses the menu and calls the BWG
// handler to start the Gemini flow.
TEST_F(PageActionMenuViewControllerTest, GeminiButtonTapped) {
  PageActionMenuContentEntryPoint* entryPoint =
      [[PageActionMenuContentEntryPoint alloc] initWithEnabled:YES];
  OCMStub([mock_mutator_ geminiEntryPoint]).andReturn(entryPoint);
  OCMStub([mock_mutator_ shouldShowFeatureEntryPoints]).andReturn(YES);
  [view_controller_ loadViewIfNeeded];

  OCMExpect([mock_page_action_menu_handler_
                dismissPageActionMenuWithCompletion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion)(void);
        [invocation getArgument:&completion atIndex:2];
        if (completion) {
          completion();
        }
      });
  OCMExpect([mock_bwg_handler_ startGeminiFlowWithStartupState:[OCMArg any]]);

  UIButton* bwgButton = (UIButton*)FindViewByAccessibilityIdentifier(
      view_controller_.view, kAIHubAskGeminiButtonAccessibilityIdentifier);
  [view_controller_ handleGeminiTapped:bwgButton];

  // `self` is needed by OCMVerifyAll macro in C++ tests.
  id self = nil;
  OCMVerifyAll(mock_page_action_menu_handler_);
  OCMVerifyAll(mock_bwg_handler_);
}

// Tests that loading the view with ineligibility reasons logs impressions.
TEST_F(PageActionMenuViewControllerTest, FooterRowShownMetric) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kPageToolsFeatureUnavailability);
  base::HistogramTester histogram_tester;
  ContentEntryPointUnavailabilityItem* item =
      [ContentEntryPointUnavailabilityItem geminiEnterprise];
  OCMStub([mock_mutator_ unavailabilityItemsForTraitCollection:[OCMArg any]])
      .andReturn(@[ item ]);

  [view_controller_ loadViewIfNeeded];
  [view_controller_ updateFooterContent];
  EXPECT_GT(histogram_tester.GetBucketCount(
                "IOS.PageActionMenu.Footer.RowShown",
                IOSPageActionMenuFooterReason::kGeminiEnterprise),
            0);
}

// Tests that loading the view without ineligibility reasons doesn't log
// impressions.
TEST_F(PageActionMenuViewControllerTest, FooterRowNotShownMetric) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kPageToolsFeatureUnavailability);
  base::HistogramTester histogram_tester;
  OCMStub([mock_mutator_ unavailabilityItemsForTraitCollection:[OCMArg any]])
      .andReturn(@[]);

  [view_controller_ loadViewIfNeeded];
  [view_controller_ updateFooterContent];
  histogram_tester.ExpectTotalCount("IOS.PageActionMenu.Footer.RowShown", 0);
}
