// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <string>

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/browser/passwords/password_breach/public/password_breach_constants.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

namespace {

constexpr char kInputPage[] = "Input";
constexpr char kInputElement[] = "input";

id<GREYMatcher> PasswordProtectionMatcher() {
  return grey_accessibilityID(kPasswordProtectionViewAccessibilityIdentifier);
}

// Request handler for net::EmbeddedTestServer that serves a simple input
// textfield.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (request.relative_url.find("bypass=true") != std::string::npos) {
    http_response->set_content(
        "Input: <input type='text' id='input'>"
        "<script>"
        "  document.getElementById('input').addEventListener('keydown', "
        "function(e) {"
        "    if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 'v') {"
        "      e.preventDefault();"
        "      navigator.clipboard.readText().then(text => {"
        "        document.getElementById('input').value = text;"
        "      });"
        "    }"
        "  });"
        "</script>");
  } else if (request.relative_url.find("preventDefault=true") !=
             std::string::npos) {
    http_response->set_content(
        "Input: <input type='text' id='input'>"
        "<script>"
        "  document.getElementById('input').addEventListener('keydown', "
        "function(e) {"
        "    e.preventDefault();"
        "    if (e.key.length === 1) {"
        "      document.getElementById('input').value += e.key;"
        "    }"
        "  });"
        "</script>");
  } else {
    http_response->set_content("Input: <input type='text' id='input'>");
  }
  http_response->set_content_type("text/html");
  return http_response;
}
}  // namespace

// Tests PhishGuard saved password reuse protection.
@interface PasswordProtectionTestCase : ChromeTestCase {
  // A URL that is treated as an unsafe phishing page by PhishGuard.
  GURL _phishingURL;
  // A URL that is allow listed by PhishGuard.
  GURL _allowlistedURL;
}
@end

@implementation PasswordProtectionTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Use `ForceRelaunchByCleanShutdown` to ensure a clean app state for each
  // test. Some tests show a modal warning which cannot be reliably dismissed
  // programmatically.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // Use commandline args to save a fake allowlisted URL.
  config.additional_args.push_back(
      std::string("--mark_as_allowlisted_for_phish_guard=") +
      _allowlistedURL.spec());

  if ([self isRunningTest:@selector(testPasswordReuseDetectionWarning)] ||
      [self
          isRunningTest:@selector(
                            testPasswordReuseDetectionKeydownPreventDefault)] ||
      [self isRunningTest:@selector(testPasswordReuseDetectionPaste)] ||
      [self isRunningTest:@selector(testPasswordReuseDetectionPasteBypass)] ||
      [self
          isRunningTest:
              @selector(testPasswordReuseDetectionPasteWithKeyboardShortcut)]) {
    // Use commandline args to save a fake phishing cached verdict.
    config.additional_args.push_back(
        std::string("--mark_as_phish_guard_phishing=") + _phishingURL.spec());
  }

  return config;
}

- (void)setUp {
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  bool started = self.testServer->Start();
  _phishingURL = self.testServer->GetURL("/phishingURL");
  _allowlistedURL = self.testServer->GetURL("/allowlistedURL");
  [super setUp];
  GREYAssertTrue(started, @"Server did not start.");
  NSURL* URL = [NSURL URLWithString:@"http://www.example.com"];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"Username"
                                                  password:@"Password"
                                                       URL:URL];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"There should be one credential.");
}

- (void)tearDownHelper {
  GREYAssertTrue([PasswordManagerAppInterface clearCredentials],
                 @"Clearing credentials wasn't done.");
  [super tearDownHelper];
}

- (void)customWaitForKeyboardToAppear {
  ScopedSynchronizationDisabler disabler;

  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard or accessory bar to appear"
                  block:^BOOL {
                    if ([EarlGrey isKeyboardShownWithError:nil]) {
                      return YES;
                    }
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:
                                   grey_accessibilityID(
                                       kFormInputAccessoryViewAccessibilityID)]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error == nil;
                  }];
  bool success = [waitForKeyboard
      waitWithTimeout:base::test::ios::kWaitForActionTimeout.InSecondsF()];
  GREYAssertTrue(success, @"Keyboard or accessory bar didn't appear");
}

- (void)tapWebInput {
  GREYCondition* interactableCondition = [GREYCondition
      conditionWithName:@"Wait for web view to be interactable."
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                                            WebViewMatcher()]
                        assertWithMatcher:grey_interactable()
                                    error:&error];
                    return !error;
                  }];
  GREYAssertTrue([interactableCondition
                     waitWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                         .InSecondsF()],
                 @"Web view did not become interactable.");

  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:kInputElement]];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kInputElement)];
}

- (void)tapWebInputAndWaitForKeyboard {
  [self tapWebInput];
  [self customWaitForKeyboardToAppear];
}

- (void)typePasswordIntoWebInput {
  [self tapWebInputAndWaitForKeyboard];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"P" flags:UIKeyModifierShift];
  for (NSString* character in @[ @"a", @"s", @"s", @"w", @"o", @"r", @"d" ]) {
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:character flags:0];
  }
}

- (void)waitForPasswordProtectionWarningWithoutSync {
  // Disable synchronization to instruct EarlGrey to inspect the view
  // hierarchy immediately instead of waiting for the app to become idle,
  // which can block the test and cause a timeout during the modal's
  // presentation animation on slow bots.
  ScopedSynchronizationDisabler disabler;
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:PasswordProtectionMatcher()
                                  timeout:base::test::ios::
                                              kWaitForUIElementTimeout];
}

// Tests that password protection UI is shown when saved password is reused on
// phishing site.
- (void)testPasswordReuseDetectionWarning {
  // PhishGuard is only available on iOS 14.0 or above.

  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:kInputPage];

  [self typePasswordIntoWebInput];
  [self waitForPasswordProtectionWarningWithoutSync];
}

// Tests that password protection UI is shown even when the webpage cancels
// keydown events.
- (void)testPasswordReuseDetectionKeydownPreventDefault {
  [ChromeEarlGrey loadURL:GURL(_phishingURL.spec() + "?preventDefault=true")];
  [ChromeEarlGrey waitForWebStateContainingText:kInputPage];

  [self typePasswordIntoWebInput];
  [self waitForPasswordProtectionWarningWithoutSync];
}

// Tests that password protection UI is shown even when the webpage intercepts
// Cmd+V/Ctrl+V and cancels the keydown and paste events.
- (void)testPasswordReuseDetectionPasteBypass {
  [ChromeEarlGrey
      loadURL:GURL(base::StrCat({_phishingURL.spec(), "?bypass=true"}))];
  [ChromeEarlGrey waitForWebStateContainingText:kInputPage];

  [self tapWebInput];

  ScopedSynchronizationDisabler disabler;

  // Copy password to clipboard and simulate Cmd+V paste.
  [ChromeEarlGrey copyTextToPasteboard:@"Password"];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"v"
                                          flags:UIKeyModifierCommand];

  [self waitForPasswordProtectionWarningWithoutSync];
}

// Tests that password protection UI is not shown when saved password is reused
// on safe site.
- (void)testPasswordProtectionNotShownForAllowListedURL {
  // PhishGuard is only available on iOS 14.0 or above.

  [ChromeEarlGrey loadURL:_allowlistedURL];
  [ChromeEarlGrey waitForWebStateContainingText:kInputPage];

  [self typePasswordIntoWebInput];

  [[EarlGrey selectElementWithMatcher:PasswordProtectionMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that password protection UI is shown when a saved password is pasted
// on a phishing site (using the callout menu).
- (void)testPasswordReuseDetectionPaste {
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:kInputPage];

  // Tap input once to focus it.
  [self tapWebInputAndWaitForKeyboard];

  // Add password to the pasteboard.
  [ChromeEarlGrey copyTextToPasteboard:@"Password"];

  // Tap input a second time to bring up the iOS edit menu / callout bar.
  [self tapWebInput];

  // Tap the "Paste" button in the system callout bar.
  id<GREYMatcher> pasteButton =
      chrome_test_util::SystemSelectionCalloutPasteButton();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:pasteButton];
  [[EarlGrey selectElementWithMatcher:pasteButton] performAction:grey_tap()];

  [self waitForPasswordProtectionWarningWithoutSync];
}

// Tests that password protection UI is shown when saved password is pasted on a
// phishing site using a keyboard shortcut (Cmd+V).
- (void)testPasswordReuseDetectionPasteWithKeyboardShortcut {
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:kInputPage];

  // Tap input to focus it.
  [self tapWebInputAndWaitForKeyboard];

  // Copy password to clipboard and simulate Cmd+V paste.
  [ChromeEarlGrey copyTextToPasteboard:@"Password"];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"v"
                                          flags:UIKeyModifierCommand];

  [ChromeEarlGrey
      waitForJavaScriptCondition:
          [NSString stringWithFormat:
                        @"document.getElementById('%s').value.includes('%@');",
                        kInputElement, @"Password"]];

  [self waitForPasswordProtectionWarningWithoutSync];
}

@end
