// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_handler.h"

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_delegate.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Creates a recursive string description of `menuElement`.
// Each array element is a string "depth:type:id" with
// - depth: the depth of the node in the submenus trees
// - type: "a" for action, "c" for command, "m" for submenu, "u" for others
// - id: an id (dependant on the type).
NSArray* MenuDescription(UIMenuElement* menuElement, int indent) {
  if ([menuElement isKindOfClass:[UIAction class]]) {
    UIAction* action = (UIAction*)menuElement;
    return
        @[ [NSString stringWithFormat:@"%d:a:%@", indent, action.identifier] ];
  }
  if ([menuElement isKindOfClass:[UICommand class]]) {
    UICommand* command = (UICommand*)menuElement;
    return
        @[ [NSString stringWithFormat:@"%d:c:%@", indent,
                                      NSStringFromSelector(command.action)] ];
  }
  if ([menuElement isKindOfClass:[UIMenu class]]) {
    UIMenu* menu = (UIMenu*)menuElement;
    NSMutableArray* array = [NSMutableArray
        arrayWithObject:[NSString stringWithFormat:@"%d:m:%@", indent,
                                                   menu.identifier]];
    for (UIMenuElement* child in menu.children) {
      [array addObjectsFromArray:MenuDescription(child, indent + 1)];
    }
    return array;
  }
  return @[ [NSString
      stringWithFormat:@"%d:u:%@", indent, menuElement.description] ];
}

NSString* kPageHTML = @"<html>"
                       "  <body>"
                       "    This is a simple HTML file."
                       "  </body>"
                       "</html>";
}  // namespace

// A delegate used to intercept the menu of the webView.
@interface EditMenuInteractionDelegate
    : NSObject <UIEditMenuInteractionDelegate>
@property(nonatomic, copy) NSArray* menuDescription;
@end

@implementation EditMenuInteractionDelegate

- (UIMenu*)editMenuInteraction:(UIEditMenuInteraction*)interaction
          menuForConfiguration:(UIEditMenuConfiguration*)configuration
              suggestedActions:(NSArray<UIMenuElement*>*)suggestedActions
    API_AVAILABLE(ios(16.0)) {
  NSMutableArray* descriptions = [NSMutableArray array];
  for (UIMenuElement* element in suggestedActions) {
    [descriptions addObjectsFromArray:MenuDescription(element, 0)];
  }
  self.menuDescription = descriptions;
  // Returning nil will fail the menu presentation.
  return nil;
}

@end

// A fake PartialTranslateDelegate that enables the feature installation.
@interface FakePartialTranslateDelegate : NSObject <PartialTranslateDelegate>
@end

@implementation FakePartialTranslateDelegate
- (void)handlePartialTranslateSelection {
}

- (BOOL)canHandlePartialTranslateSelection {
  return NO;
}

- (BOOL)shouldInstallPartialTranslate {
  return YES;
}
@end

// Tests that the structure of the edit menu stays the same starting with iOS16.
// These are purposed to catch future changes in the menu.
class BrowserEditMenuHandlerTest : public PlatformTest {
 public:
  BrowserEditMenuHandlerTest()
      : task_environment_(web::WebTaskEnvironment::Options::DEFAULT,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        web_client_(std::make_unique<ChromeWebClient>()) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  NSArray* GetMenuDescription() API_AVAILABLE(ios(16.0)) {
    EditMenuInteractionDelegate* delegate =
        [[EditMenuInteractionDelegate alloc] init];
    UIEditMenuInteraction* interaction =
        [[UIEditMenuInteraction alloc] initWithDelegate:delegate];
    [web_state_->GetView() addInteraction:interaction];

    UIEditMenuConfiguration* config = [UIEditMenuConfiguration
        configurationWithIdentifier:@"test.config"
                        sourcePoint:CGPointMake(10, 10)];
    [interaction presentEditMenuWithConfiguration:config];
    if (!base::test::ios::WaitUntilConditionOrTimeout(
            base::test::ios::kWaitForUIElementTimeout, ^{
              return delegate.menuDescription != nil;
            })) {
      return nil;
    };
    return delegate.menuDescription;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  UIViewController* base_view_controller_;
  ScopedKeyWindow scoped_key_window_;
};

// Test the base structure of the menu.
TEST_F(BrowserEditMenuHandlerTest, CheckBaseMenuDescription) {
  if (@available(iOS 16, *)) {
    NSMutableArray* expectedIOS16MenuDescription =
        [NSMutableArray arrayWithArray:@[
          @"0:m:com.apple.menu.standard-edit",
          @"1:c:cut:",
          @"1:c:copy:",
          @"1:c:paste:",
          @"1:c:delete:",
          @"1:c:select:",
          @"1:c:selectAll:",
          @"0:m:com.apple.menu.replace",
          @"1:c:_promptForReplace:",
          @"1:c:_transliterateChinese:",
          @"1:c:_insertDrawing:",
          @"1:c:captureTextFromCamera:",
          @"0:m:com.apple.menu.open",
          @"0:m:com.apple.menu.format",
          @"1:m:com.apple.menu.text-style",
          @"2:c:toggleBoldface:",
          @"2:c:toggleItalics:",
          @"2:c:toggleUnderline:",
          @"1:m:com.apple.menu.writing-direction",
          @"2:c:makeTextWritingDirectionRightToLeft:",
          @"2:c:makeTextWritingDirectionLeftToRight:",
          @"0:m:com.apple.menu.lookup",
          @"1:c:_findSelected:",
          @"1:c:_define:",
          @"1:c:_translate:",
          @"0:m:com.apple.menu.learn",
          @"1:c:_addShortcut:",
          @"0:m:com.apple.command.speech",
          @"1:c:_accessibilitySpeak:",
          @"1:c:_accessibilitySpeakLanguageSelection:",
          @"1:c:_accessibilityPauseSpeaking:",
          @"0:m:com.apple.menu.share",
          @"1:c:_share:"
        ]];
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      [expectedIOS16MenuDescription insertObject:@"1:c:_openInNewCanvas:"
                                         atIndex:13];
    }
    [base_view_controller_.view addSubview:web_state_->GetView()];
    web::test::LoadHtml(kPageHTML, web_state_.get());

    EXPECT_NSEQ(expectedIOS16MenuDescription, GetMenuDescription());
  }
}

// Test the structure of the menu with Chrome actions.
TEST_F(BrowserEditMenuHandlerTest, CheckCustomizedMenuDescription) {
  if (@available(iOS 16, *)) {
    NSMutableArray* expectedCustomMenuDescription =
        [NSMutableArray arrayWithArray:@[
          @"0:m:com.apple.menu.standard-edit",
          @"1:c:cut:",
          @"1:c:copy:",
          @"1:c:paste:",
          @"1:c:delete:",
          @"1:c:select:",
          @"1:c:selectAll:",
          @"0:m:com.apple.menu.replace",
          @"1:c:_promptForReplace:",
          @"1:c:_transliterateChinese:",
          @"1:c:_insertDrawing:",
          @"1:c:captureTextFromCamera:",
          @"0:m:com.apple.menu.open",
          @"0:m:com.apple.menu.format",
          @"1:m:com.apple.menu.text-style",
          @"2:c:toggleBoldface:",
          @"2:c:toggleItalics:",
          @"2:c:toggleUnderline:",
          @"1:m:com.apple.menu.writing-direction",
          @"2:c:makeTextWritingDirectionRightToLeft:",
          @"2:c:makeTextWritingDirectionLeftToRight:",
          @"0:m:com.apple.menu.lookup",
          @"1:c:_findSelected:",
          @"1:c:_define:",
          @"1:c:chromePartialTranslate:",
          @"0:m:com.apple.menu.learn",
          @"1:c:_addShortcut:",
          @"0:m:com.apple.command.speech",
          @"1:c:_accessibilitySpeak:",
          @"1:c:_accessibilitySpeakLanguageSelection:",
          @"1:c:_accessibilityPauseSpeaking:",
          @"0:m:com.apple.menu.share",
          @"1:c:_share:",
          @"0:m:chromecommand.linktotext",
          @"1:c:linkToText:"
        ]];
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      [expectedCustomMenuDescription insertObject:@"1:c:_openInNewCanvas:"
                                          atIndex:13];
    }
    base::test::ScopedFeatureList feature_list_;
    feature_list_.InitWithFeatures(
        {kIOSEditMenuPartialTranslate, kIOSCustomBrowserEditMenu}, {});
    FakePartialTranslateDelegate* translate_delegate =
        [[FakePartialTranslateDelegate alloc] init];
    BrowserEditMenuHandler* handler = [[BrowserEditMenuHandler alloc] init];
    handler.partialTranslateDelegate = translate_delegate;
    BrowserContainerViewController* container_vc =
        [[BrowserContainerViewController alloc] init];
    container_vc.browserEditMenuHandler = handler;
    [container_vc willMoveToParentViewController:base_view_controller_];
    [base_view_controller_ addChildViewController:container_vc];
    [base_view_controller_.view addSubview:container_vc.view];
    [container_vc didMoveToParentViewController:base_view_controller_];

    [container_vc setContentView:web_state_->GetView()];
    web::test::LoadHtml(kPageHTML, web_state_.get());

    EXPECT_NSEQ(expectedCustomMenuDescription, GetMenuDescription());
  }
}
