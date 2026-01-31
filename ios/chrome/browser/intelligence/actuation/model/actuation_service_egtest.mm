// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_app_interface.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

namespace {

// Returns a simple HTML page with "Hello".
std::unique_ptr<net::test_server::HttpResponse> HelloResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content("<html><body>Hello</body></html>");
  return response;
}

}  // namespace

@interface ActuationServiceTestCase : ChromeTestCase
@end

@implementation ActuationServiceTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kActuationTools);
  return config;
}

- (void)testNavigateTool_worksOnForegroundTab {
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HelloResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  const GURL destinationURL = self.testServer->GetURL("/hello");
  std::string urlString = destinationURL.spec();

  optimization_guide::proto::Action action;
  optimization_guide::proto::NavigateAction* navigateAction =
      action.mutable_navigate();
  navigateAction->set_url(urlString);
  navigateAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  std::string serializedAction;
  action.SerializeToString(&serializedAction);
  NSData* actionData = [NSData dataWithBytes:serializedAction.data()
                                      length:serializedAction.length()];

  __block NSError* executionError = nil;
  __block BOOL actionCompleted = NO;

  [ActuationAppInterface executeActionWithProto:actionData
                                     completion:^(NSError* error) {
                                       executionError = error;
                                       actionCompleted = YES;
                                     }];

  BOOL success = [[GREYCondition conditionWithName:@"Wait for action completion"
                                             block:^BOOL {
                                               return actionCompleted;
                                             }] waitWithTimeout:10.0];

  GREYAssertTrue(success, @"Action timed out.");
  GREYAssertNil(executionError, @"Action failed: %@", executionError);

  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];
}

- (void)testNavigateTool_worksOnBackgroundTab {
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HelloResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  const GURL destinationURL = self.testServer->GetURL("/hello");
  std::string urlString = destinationURL.spec();

  // Keep track of the current tab, which will become the background tab.
  NSString* backgroundTabID = [ChromeEarlGrey currentTabID];

  // Open a new tab and navigate to a test page to easily distinguish from the
  // initial tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo?Google")];

  optimization_guide::proto::Action action;
  optimization_guide::proto::NavigateAction* navigateAction =
      action.mutable_navigate();
  navigateAction->set_url(urlString);
  navigateAction->set_tab_id(backgroundTabID.intValue);

  std::string serializedAction;
  action.SerializeToString(&serializedAction);
  NSData* actionData = [NSData dataWithBytes:serializedAction.data()
                                      length:serializedAction.length()];

  __block NSError* executionError = nil;
  __block BOOL actionCompleted = NO;

  [ActuationAppInterface executeActionWithProto:actionData
                                     completion:^(NSError* error) {
                                       executionError = error;
                                       actionCompleted = YES;
                                     }];

  BOOL success = [[GREYCondition conditionWithName:@"Wait for action completion"
                                             block:^BOOL {
                                               return actionCompleted;
                                             }] waitWithTimeout:10.0];

  GREYAssertTrue(success, @"Action timed out.");
  GREYAssertNil(executionError, @"Action failed: %@", executionError);

  // Verify that the browser switched back to the initial tab.
  GREYAssertEqualObjects([ChromeEarlGrey currentTabID], backgroundTabID,
                         @"Failed to switch to the target tab.");
  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];
}

@end
