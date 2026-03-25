// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/base_paths.h"
#import "base/path_service.h"
#import "base/strings/escape.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_app_interface.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/url_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

namespace {

// Returns a simple HTML page with the content specified in the "content" query
// parameter.
std::unique_ptr<net::test_server::HttpResponse> EchoResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url.find("/echo") != 0) {
    return nullptr;
  }
  std::string content;
  if (!net::GetValueForKeyInQuery(request.GetURL(), "content", &content)) {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(
      base::StringPrintf("<html><body>%s</body></html>", content.c_str()));
  return response;
}

std::pair<const optimization_guide::proto::ContentNode*, std::string>
FindNodeWithText(const optimization_guide::proto::ContentNode& node,
                 const std::string& text,
                 const std::string& current_frame_token) {
  std::string frame_token = current_frame_token;
  if (node.content_attributes().has_iframe_data()) {
    frame_token = node.content_attributes()
                      .iframe_data()
                      .frame_data()
                      .document_identifier()
                      .serialized_token();
  }

  if (node.content_attributes().has_text_data() &&
      node.content_attributes().text_data().text_content() == text) {
    return {&node, frame_token};
  }
  for (const auto& child : node.children_nodes()) {
    auto [found, token] = FindNodeWithText(child, text, frame_token);
    if (found) {
      return {found, token};
    }
  }
  return {nullptr, ""};
}

}  // namespace

@interface ActuationServiceTestCase : ChromeTestCase
@end

@implementation ActuationServiceTestCase {
  std::unique_ptr<net::test_server::EmbeddedTestServer> _crossOriginServer;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kActuationTools);
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(base::BindRepeating(&EchoResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  _crossOriginServer = std::make_unique<net::test_server::EmbeddedTestServer>();
  _crossOriginServer->RegisterRequestHandler(
      base::BindRepeating(&EchoResponse));
  _crossOriginServer->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS)
          .AppendASCII("ios/testing/data/http_server_files/"));
  GREYAssertTrue(_crossOriginServer->Start(),
                 @"Cross origin server failed to start.");
}

#pragma mark - Helpers

// Returns a URL for the given `html` content using the given `server`.
- (GURL)URLForHTML:(const std::string&)html
            server:(net::test_server::EmbeddedTestServer*)server {
  GURL url = server->GetURL("/echo");
  std::string escaped_content = base::EscapeQueryParamValue(html, true);
  GURL::Replacements replacements;
  std::string query = "content=" + escaped_content;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

// Returns a URL for the given `html` content using the embedded test server.
- (GURL)URLForHTML:(const std::string&)html {
  return [self URLForHTML:html server:self.testServer];
}

// Executes the given `action` and waits for completion.
- (void)executeAction:(const optimization_guide::proto::Action&)action {
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
}

#pragma mark - Tests

- (void)testNavigateTool_worksOnForegroundTab {
  const GURL destinationURL = [self URLForHTML:"Hello"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::NavigateAction* navigateAction =
      action.mutable_navigate();
  navigateAction->set_url(destinationURL.spec());
  navigateAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  [self executeAction:action];

  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];
}

- (void)testNavigateTool_worksOnBackgroundTab {
  const GURL destinationURL = [self URLForHTML:"Hello"];

  // Open a new tab and navigate to a test page to easily distinguish from the
  // initial tab.
  NSString* backgroundTabID = [ChromeEarlGrey currentTabID];
  [ChromeEarlGrey openNewTab];
  NSString* foregroundTabID = [ChromeEarlGrey currentTabID];
  [ChromeEarlGrey loadURL:[self URLForHTML:"Google"]];
  [ChromeEarlGrey waitForWebStateContainingText:"Google"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::NavigateAction* navigateAction =
      action.mutable_navigate();
  navigateAction->set_url(destinationURL.spec());
  navigateAction->set_tab_id(backgroundTabID.intValue);

  [self executeAction:action];

  // Verify that the browser did not change the active tab.
  GREYAssertEqualObjects(
      [ChromeEarlGrey currentTabID], foregroundTabID,
      @"Navigating the background tab changed the active tab.");

  // Switch back to the background tab to verify the navigation.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];
}

- (void)testClickTool_clicksByCoordinates {
  const std::string buttonHTML =
      "<button onclick='this.innerText=\"Clicked\"'>Click Me</button>";

  [ChromeEarlGrey loadURL:[self URLForHTML:buttonHTML]];
  [ChromeEarlGrey waitForWebStateContainingText:"Click Me"];

  NSString* script = base::SysUTF8ToNSString(
      "(function() {"
      "  const rect = "
      "document.querySelector('button')?.getBoundingClientRect();"
      "  return {x: Math.round(rect.x + rect.width / 2), y: Math.round(rect.y "
      "+ rect.height / 2)};"
      "})();");
  base::Value result = [ChromeEarlGrey evaluateJavaScript:script];
  GREYAssertTrue(result.is_dict(), @"Result is not a dict");

  std::optional<double> x_opt = result.GetDict().FindDouble("x");
  GREYAssertTrue(x_opt.has_value(), @"x coordinate not found");
  int x = static_cast<int>(x_opt.value());

  std::optional<double> y_opt = result.GetDict().FindDouble("y");
  GREYAssertTrue(y_opt.has_value(), @"y coordinate not found");
  int y = static_cast<int>(y_opt.value());

  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* clickAction = action.mutable_click();
  clickAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickAction->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickAction->set_click_count(optimization_guide::proto::ClickAction::SINGLE);

  optimization_guide::proto::ActionTarget* target =
      clickAction->mutable_target();
  target->mutable_coordinate()->set_x(x);
  target->mutable_coordinate()->set_y(y);

  [self executeAction:action];

  [ChromeEarlGrey waitForWebStateContainingText:"Clicked"];
}

- (void)testClickTool_clicksByIdentifiers {
  const std::string buttonHTML =
      "<button onclick='this.innerText=\"Clicked\"'>Click Me</button>";
  const std::string iframeURL = [self URLForHTML:buttonHTML].spec();
  const std::string iframeHTML =
      base::StringPrintf("<iframe src='%s'></iframe>", iframeURL.c_str());

  [ChromeEarlGrey loadURL:[self URLForHTML:iframeHTML]];
  [ChromeEarlGrey waitForWebStateFrameContainingText:"Click Me"];

  NSData* apc_data = [ActuationAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext page_context;
  GREYAssertTrue(
      page_context.ParseFromArray([apc_data bytes], [apc_data length]),
      @"Failed to parse PageContext");

  auto [node, token] = FindNodeWithText(
      page_context.annotated_page_content().root_node(), "Click Me", "");
  GREYAssertTrue(node != nullptr, @"Failed to find button node");
  NSString* frameToken = base::SysUTF8ToNSString(token);
  int nodeId = node->content_attributes().common_ancestor_dom_node_id();

  GREYAssertNotNil(frameToken, @"Failed to get frame token.");
  GREYAssertTrue(nodeId > 0, @"Failed to get node ID.");

  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* clickAction = action.mutable_click();
  clickAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickAction->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickAction->set_click_count(optimization_guide::proto::ClickAction::SINGLE);

  optimization_guide::proto::ActionTarget* target =
      clickAction->mutable_target();
  target->set_content_node_id(nodeId);
  target->mutable_document_identifier()->set_serialized_token(
      base::SysNSStringToUTF8(frameToken));

  [self executeAction:action];

  [ChromeEarlGrey waitForWebStateFrameContainingText:"Clicked"];
}

- (void)testClickTool_worksOnCrossOriginIframe {
  const std::string buttonHTML =
      "<button onclick='this.innerText=\"Clicked\"'>Click Me</button>";
  const std::string iframeURL =
      [self URLForHTML:buttonHTML server:_crossOriginServer.get()].spec();
  const std::string iframeHTML =
      base::StringPrintf("<iframe src='%s'></iframe>", iframeURL.c_str());

  [ChromeEarlGrey loadURL:[self URLForHTML:iframeHTML]];
  [ChromeEarlGrey waitForWebStateFrameContainingText:"Click Me"];

  NSData* apc_data = [ActuationAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext page_context;
  GREYAssertTrue(
      page_context.ParseFromArray([apc_data bytes], [apc_data length]),
      @"Failed to parse PageContext");

  std::string frame_token = page_context.annotated_page_content()
                                .main_frame_data()
                                .document_identifier()
                                .serialized_token();
  auto [node, token] =
      FindNodeWithText(page_context.annotated_page_content().root_node(),
                       "Click Me", frame_token);
  GREYAssertTrue(node != nullptr, @"Failed to find button node");

  NSString* nsFrameToken = base::SysUTF8ToNSString(token);
  int nodeId = node->content_attributes().common_ancestor_dom_node_id();

  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* clickAction = action.mutable_click();
  clickAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickAction->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickAction->set_click_count(optimization_guide::proto::ClickAction::SINGLE);

  optimization_guide::proto::ActionTarget* target =
      clickAction->mutable_target();
  target->set_content_node_id(nodeId);
  target->mutable_document_identifier()->set_serialized_token(
      base::SysNSStringToUTF8(nsFrameToken));

  [self executeAction:action];

  [ChromeEarlGrey waitForWebStateFrameContainingText:"Clicked"];
}

@end
