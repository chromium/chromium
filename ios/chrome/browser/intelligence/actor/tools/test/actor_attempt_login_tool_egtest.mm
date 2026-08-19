// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "ios/chrome/browser/device_reauth/test/reauthentication_app_interface.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_tools_base_test_case.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@interface ActorAttemptLoginToolTestCase : ActorToolsBaseTestCase
@end

@implementation ActorAttemptLoginToolTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(password_manager::features::kActorLogin);
  return config;
}

- (void)tearDownHelper {
  [PasswordManagerAppInterface clearCredentials];
  [super tearDownHelper];
}

#pragma mark - Tests

// Tests that the `AttemptLoginTool` fills the login form with existing
// credentials from Password Manager, assuming that device authentication is
// enabled.
- (void)testAttemptLoginTool_fillsLoginForm_Reauth {
  // Establish reauth.
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:YES];
  [ReauthenticationAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  // Establish login form HTML.
  // Loads simple page. It is on localhost so it is considered a secure context.
  GURL url = self.testServer->GetURL("/simple_login_form_empty.html");

  // Inject one credential for this page's URL in the password store.
  NSString* username = @"test_user";
  NSString* password = @"test_password";

  // Clear any existing credentials before starting the test.
  [PasswordManagerAppInterface clearCredentials];

  NSError* storeError = [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(url)];
  GREYAssertNil(storeError, @"Failed to store credential");

  // Load the page and wait for the form to appear.
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];

  // Construct AttemptLoginAction.
  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptLoginAction* attemptLogin =
      action.mutable_attempt_login();
  attemptLogin->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  optimization_guide::proto::AttemptLoginAction_LoginTarget* loginTarget =
      attemptLogin->add_login_targets();
  loginTarget->set_login_type(
      optimization_guide::proto::AttemptLoginAction_LoginTarget::
          PASSWORD_FORM_SUBMIT);
  [self setCoordinatesOnTarget:loginTarget->mutable_target()
                  withSelector:"#submit_button"];

  // Execute the action asynchronously (as it blocks waiting for reauth).
  std::string serializedAction;
  action.SerializeToString(&serializedAction);
  NSData* actionData = [NSData dataWithBytes:serializedAction.data()
                                      length:serializedAction.length()];

  __block NSError* executionError = nil;
  __block BOOL actionCompleted = NO;

  [ActorAppInterface executeActionWithProto:actionData
                                 completion:^(NSError* error) {
                                   executionError = error;
                                   actionCompleted = YES;
                                 }];

  // Verify that the login form was NOT filled before reauth is passed.
  NSString* notFilledCondition =
      @"document.getElementById('un')?.value === '' && "
       "document.getElementById('pw')?.value === ''";
  [ChromeEarlGrey waitForJavaScriptCondition:notFilledCondition];

  // Manually trigger the successful re-authentication result.
  [ReauthenticationAppInterface mockReauthenticationModuleReturnMockedResult];

  // Wait for the action to complete.
  BOOL success = [[GREYCondition conditionWithName:@"Wait for action completion"
                                             block:^BOOL {
                                               return actionCompleted;
                                             }] waitWithTimeout:10.0];

  GREYAssertTrue(success, @"Action timed out.");
  GREYAssertNil(executionError, @"Action failed: %@", executionError);

  // Verify that the login form was filled.
  NSString* condition = [NSString
      stringWithFormat:@"document.getElementById('%s')?.value === '%@' && "
                       @"!!document.getElementById('%s')?.value",
                       "un", username, "pw"];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Tests that the `AttemptLoginTool` fills the login form with existing
// credentials from Password Manager, assuming that device authentication is
// disabled.
- (void)testAttemptLoginTool_fillsLoginForm_NoReauth {
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:NO];

  // Establish login form HTML.
  // Loads simple page. It is on localhost so it is considered a secure context.
  GURL url = self.testServer->GetURL("/simple_login_form_empty.html");

  // Inject one credential for this page's URL in the password store.
  NSString* username = @"test_user";
  NSString* password = @"test_password";

  // Clear any existing credentials before starting the test.
  [PasswordManagerAppInterface clearCredentials];

  NSError* storeError = [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(url)];
  GREYAssertNil(storeError, @"Failed to store credential");

  // Load the page and wait for the form to appear.
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];

  // Construct AttemptLoginAction.
  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptLoginAction* attemptLogin =
      action.mutable_attempt_login();
  attemptLogin->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  optimization_guide::proto::AttemptLoginAction_LoginTarget* loginTarget =
      attemptLogin->add_login_targets();
  loginTarget->set_login_type(
      optimization_guide::proto::AttemptLoginAction_LoginTarget::
          PASSWORD_FORM_SUBMIT);
  [self setCoordinatesOnTarget:loginTarget->mutable_target()
                  withSelector:"#submit_button"];

  // Execute the action and wait for execution.
  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  // Verify that the login form was filled.
  NSString* condition = [NSString
      stringWithFormat:@"document.getElementById('%s')?.value === '%@' && "
                       @"!!document.getElementById('%s')?.value",
                       "un", username, "pw"];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Tests that the `AttemptLoginTool` scrolls and fills the login form with
// existing credentials from Password Manager when the form is located further
// down the page.
- (void)testAttemptLoginTool_scrollsAndFillsLoginForm {
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:NO];

  // Establish login form HTML.
  std::string html =
      "<div style=\"width: 100px; height: 2000px; background-color: "
      "gray;\">Long Advertisement</div>"
      "Login form."
      "<form name='login_form' id='login_form'>"
      "  <input type='text' name='username' id='un'><br/>"
      "  <input type='password' name='password' id='pw'><br/>"
      "  <button id='submit_button' value='Submit'>SubForm</button>"
      "</form>";
  GURL url = [self URLForHTML:html];

  // Inject one credential for this page's URL in the password store.
  NSString* username = @"test_user";
  NSString* password = @"test_password";

  // Clear any existing credentials before starting the test.
  [PasswordManagerAppInterface clearCredentials];

  NSError* storeError = [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(url)];
  GREYAssertNil(storeError, @"Failed to store credential");

  // Load the page and wait for the form to appear.
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];

  // Construct AttemptLoginAction.
  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptLoginAction* attemptLogin =
      action.mutable_attempt_login();
  attemptLogin->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  optimization_guide::proto::AttemptLoginAction_LoginTarget* loginTarget =
      attemptLogin->add_login_targets();
  loginTarget->set_login_type(
      optimization_guide::proto::AttemptLoginAction_LoginTarget::
          PASSWORD_FORM_SUBMIT);
  [self setCoordinatesOnTarget:loginTarget->mutable_target()
                  withSelector:"#submit_button"];

  // Execute the action and wait for execution.
  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  // Verify that the login form was filled.
  NSString* condition = [NSString
      stringWithFormat:@"document.getElementById('%s')?.value === '%@' && "
                       @"!!document.getElementById('%s')?.value",
                       "un", username, "pw"];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Tests that when device reauthentication is disabled, triggering the
// AttemptLogin action and immediately switching to a new tab does not block.
// The form in the background tab is filled successfully, and is already filled
// when the user switches back.
- (void)testAttemptLoginTool_tabSwitch_NoReauth {
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:NO];

  // Load page and inject credential.
  GURL url = self.testServer->GetURL("/simple_login_form_empty.html");
  NSString* username = @"test_user";
  NSString* password = @"test_password";
  [PasswordManagerAppInterface clearCredentials];
  NSError* storeError = [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(url)];
  GREYAssertNil(storeError, @"Failed to store credential");

  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];

  // Construct action.
  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptLoginAction* attemptLogin =
      action.mutable_attempt_login();
  attemptLogin->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  optimization_guide::proto::AttemptLoginAction_LoginTarget* loginTarget =
      attemptLogin->add_login_targets();
  loginTarget->set_login_type(
      optimization_guide::proto::AttemptLoginAction_LoginTarget::
          PASSWORD_FORM_SUBMIT);
  [self setCoordinatesOnTarget:loginTarget->mutable_target()
                  withSelector:"#submit_button"];

  // Start action asynchronously.
  std::string serializedAction;
  action.SerializeToString(&serializedAction);
  NSData* actionData = [NSData dataWithBytes:serializedAction.data()
                                      length:serializedAction.length()];

  __block NSError* executionError = nil;
  __block BOOL actionCompleted = NO;

  [ActorAppInterface executeActionWithProto:actionData
                                 completion:^(NSError* error) {
                                   executionError = error;
                                   actionCompleted = YES;
                                 }];

  // Immediately open a new tab and wait for a few seconds.
  [ChromeEarlGrey openNewTab];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(5));

  // Verify the action has completed successfully.
  GREYAssertTrue(actionCompleted,
                 @"Action should have completed in background");
  GREYAssertNil(executionError, @"Action failed in background: %@",
                executionError);

  // Go back to original tab.
  [ChromeEarlGrey selectTabAtIndex:0];

  // Verify the form is already filled.
  NSString* condition = [NSString
      stringWithFormat:@"document.getElementById('%s')?.value === '%@' && "
                       @"!!document.getElementById('%s')?.value",
                       "un", username, "pw"];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Tests that when device reauthentication is enabled, triggering the
// AttemptLogin action and immediately switching to a new tab causes the
// execution to remain pending (not filled) while in the background. Once the
// user switches back to the original tab, device reauthentication is prompted
// and the form is filled after reauth succeeds.
- (void)testAttemptLoginTool_tabSwitch_Reauth {
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:YES];
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  [ReauthenticationAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  // Load page and inject credential.
  GURL url = self.testServer->GetURL("/simple_login_form_empty.html");
  NSString* username = @"test_user";
  NSString* password = @"test_password";
  [PasswordManagerAppInterface clearCredentials];
  NSError* storeError = [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(url)];
  GREYAssertNil(storeError, @"Failed to store credential");

  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];

  optimization_guide::proto::Action action;
  optimization_guide::proto::AttemptLoginAction* attemptLogin =
      action.mutable_attempt_login();
  attemptLogin->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  optimization_guide::proto::AttemptLoginAction_LoginTarget* loginTarget =
      attemptLogin->add_login_targets();
  loginTarget->set_login_type(
      optimization_guide::proto::AttemptLoginAction_LoginTarget::
          PASSWORD_FORM_SUBMIT);
  [self setCoordinatesOnTarget:loginTarget->mutable_target()
                  withSelector:"#submit_button"];

  // Start action asynchronously.
  std::string serializedAction;
  action.SerializeToString(&serializedAction);
  NSData* actionData = [NSData dataWithBytes:serializedAction.data()
                                      length:serializedAction.length()];

  __block NSError* executionError = nil;
  __block BOOL actionCompleted = NO;

  [ActorAppInterface executeActionWithProto:actionData
                                 completion:^(NSError* error) {
                                   executionError = error;
                                   actionCompleted = YES;
                                 }];

  // Immediately open a new tab and wait.
  [ChromeEarlGrey openNewTab];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(5));

  // Verify the action has NOT completed yet because the tab is in the
  // background and reauth is pending.
  GREYAssertFalse(actionCompleted,
                  @"Action should not have completed in background");

  // Go back to original tab and verify that the login form is still NOT filled
  // before reauth is passed.
  [ChromeEarlGrey selectTabAtIndex:0];
  NSString* notFilledCondition =
      @"document.getElementById('un')?.value === '' && "
       "document.getElementById('pw')?.value === ''";
  [ChromeEarlGrey waitForJavaScriptCondition:notFilledCondition];

  // Manually trigger the successful re-authentication result, and wait for the
  // action to complete.
  [ReauthenticationAppInterface mockReauthenticationModuleReturnMockedResult];
  BOOL success = [[GREYCondition conditionWithName:@"Wait for action completion"
                                             block:^BOOL {
                                               return actionCompleted;
                                             }] waitWithTimeout:10.0];

  GREYAssertTrue(success, @"Action timed out after reauth");
  GREYAssertNil(executionError, @"Action failed: %@", executionError);

  // Verify that the form is filled.
  NSString* condition = [NSString
      stringWithFormat:@"document.getElementById('%s')?.value === '%@' && "
                       @"!!document.getElementById('%s')?.value",
                       "un", username, "pw"];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

@end
