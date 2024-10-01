// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_handler.h"

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/link_to_text/ui_bundled/link_to_text_mediator.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_mediator.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/providers/partial_translate/test_partial_translate.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

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
  if ([menuElement isKindOfClass:[UIDeferredMenuElement class]]) {
    // It is not possible to have info from UIDeferredMenuElement as they
    // are only a block.
    return @[ [NSString stringWithFormat:@"%d:d", indent] ];
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

// Return the base menu depending on the environment.
NSMutableArray* GetExpectedMenu() {
  if (@available(iOS 18, *)) {
    return [NSMutableArray arrayWithArray:@[
      @"0:m:com.apple.menu.standard-edit",
      @"1:c:cut:",
      @"1:c:copy:",
      @"1:c:paste:",
      @"1:c:delete:",
      @"1:c:select:",
      @"1:c:selectAll:",
      @"0:m:com.apple.menu.replace",
      @"1:c:promptForReplace:",
      @"1:c:transliterateChinese:",
      @"1:c:_insertDrawing:",
      @"1:m:com.apple.menu.autofill",
      @"2:m:com.apple.menu.insert-from-external-sources",
      @"2:c:captureTextFromCamera:",
      @"1:c:_startWritingTools:",
      @"0:m:com.apple.menu.open",
      @"0:m:com.apple.menu.format",
      @"1:m:com.apple.menu.text-style",
      @"2:c:toggleBoldface:",
      @"2:c:toggleItalics:",
      @"2:c:toggleUnderline:",
      @"1:m:com.apple.menu.writing-direction",
      @"2:c:makeTextWritingDirectionRightToLeft:",
      @"2:c:makeTextWritingDirectionLeftToRight:",
      @"1:c:_showTextFormattingOptions:",
      @"0:m:com.apple.menu.lookup",
      @"1:c:findSelected:",
      @"1:c:_define:",
      @"1:c:_translate:",
      @"0:m:com.apple.menu.learn",
      @"1:c:addShortcut:",
      @"0:m:com.apple.command.speech",
      @"1:c:_accessibilitySpeak:",
      @"1:c:_accessibilitySpeakLanguageSelection:",
      @"1:c:_accessibilityPauseSpeaking:",
      @"0:m:com.apple.menu.share",
      @"1:c:share:"
    ]];
  } else if (@available(iOS 17.4, *)) {
    return [NSMutableArray arrayWithArray:@[
      @"0:m:com.apple.menu.standard-edit",
      @"1:c:cut:",
      @"1:c:copy:",
      @"1:c:paste:",
      @"1:c:delete:",
      @"1:c:select:",
      @"1:c:selectAll:",
      @"0:m:com.apple.menu.replace",
      @"1:c:promptForReplace:",
      @"1:c:transliterateChinese:",
      @"1:c:_insertDrawing:",
      @"1:m:com.apple.menu.autofill",
      @"2:m:com.apple.menu.insert-from-external-sources",
      @"2:c:captureTextFromCamera:",
      @"0:m:com.apple.menu.open",
      @"0:m:com.apple.menu.format",
      @"1:m:com.apple.menu.text-style",
      @"2:c:toggleBoldface:",
      @"2:c:toggleItalics:",
      @"2:c:toggleUnderline:",
      @"1:m:com.apple.menu.writing-direction",
      @"2:c:makeTextWritingDirectionRightToLeft:",
      @"2:c:makeTextWritingDirectionLeftToRight:",
      @"1:c:_showTextFormattingOptions:",
      @"0:m:com.apple.menu.lookup",
      @"1:c:findSelected:",
      @"1:c:_define:",
      @"1:c:_translate:",
      @"0:m:com.apple.menu.learn",
      @"1:c:addShortcut:",
      @"0:m:com.apple.command.speech",
      @"1:c:_accessibilitySpeak:",
      @"1:c:_accessibilitySpeakLanguageSelection:",
      @"1:c:_accessibilityPauseSpeaking:",
      @"0:m:com.apple.menu.share",
      @"1:c:share:"
    ]];
  } else if (@available(iOS 17, *)) {
    return [NSMutableArray arrayWithArray:@[
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
      @"1:m:com.apple.menu.autofill",
      @"2:m:com.apple.menu.insert-from-external-sources",
      @"2:c:captureTextFromCamera:",
      @"0:m:com.apple.menu.open",
      @"0:m:com.apple.menu.format",
      @"1:m:com.apple.menu.text-style",
      @"2:c:toggleBoldface:",
      @"2:c:toggleItalics:",
      @"2:c:toggleUnderline:",
      @"1:m:com.apple.menu.writing-direction",
      @"2:c:makeTextWritingDirectionRightToLeft:",
      @"2:c:makeTextWritingDirectionLeftToRight:",
      @"1:c:_showTextFormattingOptions:",
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
  }
  NSMutableArray* expectedMenuDescription = [NSMutableArray arrayWithArray:@[
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
  return expectedMenuDescription;
}

// Add Open In New Canvas on iPad
void AddOpenInNewCanvas(NSMutableArray* menu) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  for (unsigned int i = 0; i < menu.count; i++) {
    if ([menu[i] isEqualToString:@"0:m:com.apple.menu.format"]) {
      [menu insertObject:@"1:c:_openInNewCanvas:" atIndex:i];
      return;
    }
  }
}

// Modify the expected menu for partial translate
void AddPartialTranslate(NSMutableArray* menu) {
  for (unsigned int i = 0; i < menu.count; i++) {
    if ([menu[i] isEqualToString:@"0:m:com.apple.menu.lookup"]) {
      [menu insertObject:@"0:m:chromecommand.menu" atIndex:i];
      // Deferred action.
      [menu insertObject:@"1:d" atIndex:i + 1];
      break;
    }
  }
  for (unsigned int i = 0; i < menu.count; i++) {
    if ([menu[i] isEqualToString:@"1:c:_translate:"]) {
      [menu removeObjectAtIndex:i];
      break;
    }
  }
}

// Modify the expected menu for Link to text
void AddLinkToText(NSMutableArray* menu) {
  [menu addObjectsFromArray:@[ @"0:m:chromecommand.menu.linktotext", @"1:d" ]];
}
}  // namespace

// A fake partial translate provider.
@interface TestPartialTranslateControllerFactory
    : NSObject <PartialTranslateControllerFactory>
@end

@implementation TestPartialTranslateControllerFactory

- (id<PartialTranslateController>)
    createTranslateControllerForSourceText:(NSString*)sourceText
                                anchorRect:(CGRect)anchor
                               inIncognito:(BOOL)inIncognito {
  return nil;
}

- (NSUInteger)maximumCharacterLimit {
  return 1100;
}

@end

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

// Tests that the structure of the edit menu stays the same starting with iOS16.
// These are purposed to catch future changes in the menu.
class BrowserEditMenuHandlerTest : public PlatformTest {
 public:
  BrowserEditMenuHandlerTest()
      : web_client_(std::make_unique<ChromeWebClient>()),
        web_state_list_(&web_state_list_delegate_) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  void TearDown() override {
    // Reset the partial translate factory
    ios::provider::test::SetPartialTranslateControllerFactory(nil);
    PlatformTest::TearDown();
  }

  void SetupTranslateControllerFactory() {
    TestPartialTranslateControllerFactory* factory =
        [[TestPartialTranslateControllerFactory alloc] init];
    ios::provider::test::SetPartialTranslateControllerFactory(factory);
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
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<web::WebState> web_state_;
  UIViewController* base_view_controller_;
  ScopedKeyWindow scoped_key_window_;
};

// Test the base structure of the menu.
TEST_F(BrowserEditMenuHandlerTest, CheckBaseMenuDescription) {
  NSMutableArray* expectedMenuDescription = GetExpectedMenu();
  AddOpenInNewCanvas(expectedMenuDescription);
  [base_view_controller_.view addSubview:web_state_->GetView()];
  web::test::LoadHtml(kPageHTML, web_state_.get());

  EXPECT_NSEQ(expectedMenuDescription, GetMenuDescription());
}

// Test the structure of the menu with Chrome actions.
TEST_F(BrowserEditMenuHandlerTest, CheckCustomizedMenuDescription) {
  NSMutableArray* expectedMenuDescription = GetExpectedMenu();
  AddOpenInNewCanvas(expectedMenuDescription);
  AddPartialTranslate(expectedMenuDescription);
  AddLinkToText(expectedMenuDescription);
  SetupTranslateControllerFactory();
  PartialTranslateMediator* partial_translate_mediator =
      [[PartialTranslateMediator alloc]
            initWithWebStateList:&web_state_list_
          withBaseViewController:base_view_controller_
                     prefService:profile_->GetPrefs()
            fullscreenController:nullptr
                       incognito:NO];

  LinkToTextMediator* link_to_text_mediator =
      [[LinkToTextMediator alloc] initWithWebStateList:&web_state_list_];
  BrowserEditMenuHandler* handler = [[BrowserEditMenuHandler alloc] init];
  handler.partialTranslateDelegate = partial_translate_mediator;
  handler.linkToTextDelegate = link_to_text_mediator;
  BrowserContainerViewController* container_vc =
      [[BrowserContainerViewController alloc] init];
  container_vc.browserEditMenuHandler = handler;
  [container_vc willMoveToParentViewController:base_view_controller_];
  [base_view_controller_ addChildViewController:container_vc];
  [base_view_controller_.view addSubview:container_vc.view];
  [container_vc didMoveToParentViewController:base_view_controller_];

  [container_vc setContentView:web_state_->GetView()];
  web::test::LoadHtml(kPageHTML, web_state_.get());
  EXPECT_NSEQ(expectedMenuDescription, GetMenuDescription());
  handler.partialTranslateDelegate = nil;
  [partial_translate_mediator shutdown];
}
