// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/base_paths.h"
#import "base/functional/function_ref.h"
#import "base/path_service.h"
#import "base/strings/escape.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/url_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

namespace {

struct FindNodeResult {
  const optimization_guide::proto::ContentNode* node = nullptr;
  const optimization_guide::proto::ContentNode* parent = nullptr;
  std::string frame_token;
};

// Returns a simple HTML page with the content specified in the "content" query
// parameter.
std::unique_ptr<net::test_server::HttpResponse> EchoResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url.find("/echo") != 0) {
    return nullptr;
  }
  std::string content;
  const GURL& requestUrl = request.GetURL();
  for (net::QueryIterator it(requestUrl); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == "content") {
      content = base::UnescapeBinaryURLComponent(
          it.GetValue(), base::UnescapeRule::NORMAL |
                             base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
      break;
    }
  }
  if (content.empty()) {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(base::StringPrintf(
      R"(
            <html>
                <body>
                    %s
                </body>
            </html>
        )",
      content.c_str()));
  return response;
}

FindNodeResult FindNodeWithPredicate(
    const optimization_guide::proto::ContentNode& node,
    base::FunctionRef<bool(const optimization_guide::proto::ContentNode&)>
        predicate,
    const std::string& current_frame_token,
    const optimization_guide::proto::ContentNode* parent = nullptr) {
  std::string frame_token = current_frame_token;
  if (node.content_attributes().has_iframe_data()) {
    frame_token = node.content_attributes()
                      .iframe_data()
                      .frame_data()
                      .document_identifier()
                      .serialized_token();
  }

  if (predicate(node)) {
    return {&node, parent, frame_token};
  }
  for (const auto& child : node.children_nodes()) {
    FindNodeResult result =
        FindNodeWithPredicate(child, predicate, frame_token, &node);
    if (result.node) {
      return result;
    }
  }
  return {nullptr, nullptr, ""};
}

FindNodeResult FindNodeWithText(
    const optimization_guide::proto::ContentNode& node,
    const std::string& text,
    const std::string& current_frame_token,
    const optimization_guide::proto::ContentNode* parent = nullptr) {
  return FindNodeWithPredicate(
      node,
      [&text](const optimization_guide::proto::ContentNode& n) {
        if (n.content_attributes().has_text_data()) {
          std::string original =
              n.content_attributes().text_data().text_content();
          std::string trimmed = base::CollapseWhitespaceASCII(
              original, /**trim_sequences_with_line_breaks=*/true);
          if (trimmed == text) {
            return true;
          }
        }
        if (n.content_attributes().has_form_control_data() &&
            n.content_attributes().form_control_data().field_value() == text) {
          return true;
        }
        return false;
      },
      current_frame_token, parent);
}

}  // namespace

@interface ActorToolsTestCase : ChromeTestCase
@end

@implementation ActorToolsTestCase {
  std::unique_ptr<net::test_server::EmbeddedTestServer> _crossOriginServer;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kActorTools);
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

  [ActorAppInterface executeActionWithProto:actionData
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

// Makes a JavaScript function that will find the center coordinates of the
// element with the given selector.
- (NSString*)findCenterJsForElementWithSelector:(const std::string&)selector {
  return base::SysUTF8ToNSString(base::StringPrintf(
      R"(
      (function() {
        const rect = document.querySelector('%s').getBoundingClientRect();
        return {x: Math.round(rect.x + rect.width / 2),
                y: Math.round(rect.y + rect.height / 2)};
      })();
      )",
      selector.c_str()));
}

#pragma mark - Tests

// Tests that the navigate tool successfully navigates the active tab to a new
// URL.
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

// Tests that the navigate tool successfully navigates a background tab to a new
// URL without changing the active tab.
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

// Tests that the click tool successfully clicks an element using its
// coordinates.
- (void)testClickTool_clicksByCoordinates {
  const std::string buttonHTML =
      "<button onclick='this.innerText=\"Clicked\"'>Click Me</button>";

  [ChromeEarlGrey loadURL:[self URLForHTML:buttonHTML]];
  [ChromeEarlGrey waitForWebStateContainingText:"Click Me"];

  NSString* script = [self findCenterJsForElementWithSelector:"button"];
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

// Tests that the click tool successfully clicks an element using its DOM node
// ID and frame token.
- (void)testClickTool_clicksByIdentifiers {
  const std::string buttonHTML =
      "<button onclick='this.innerText=\"Clicked\"'>Click Me</button>";
  const std::string iframeURL = [self URLForHTML:buttonHTML].spec();
  const std::string iframeHTML =
      base::StringPrintf("<iframe src='%s'></iframe>", iframeURL.c_str());

  [ChromeEarlGrey loadURL:[self URLForHTML:iframeHTML]];
  [ChromeEarlGrey waitForWebStateFrameContainingText:"Click Me"];

  NSData* apc_data = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext page_context;
  GREYAssertTrue(
      page_context.ParseFromArray([apc_data bytes], [apc_data length]),
      @"Failed to parse PageContext");

  FindNodeResult result = FindNodeWithText(
      page_context.annotated_page_content().root_node(), "Click Me", "");
  GREYAssertTrue(result.node != nullptr, @"Failed to find button node");
  NSString* frameToken = base::SysUTF8ToNSString(result.frame_token);
  int nodeId = result.node->content_attributes().common_ancestor_dom_node_id();

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

// Tests that the click tool successfully clicks an element inside a
// cross-origin iframe.
- (void)testClickTool_worksOnCrossOriginIframe {
  const std::string buttonHTML =
      "<button onclick='this.innerText=\"Clicked\"'>Click Me</button>";
  const std::string iframeURL =
      [self URLForHTML:buttonHTML server:_crossOriginServer.get()].spec();
  const std::string iframeHTML =
      base::StringPrintf("<iframe src='%s'></iframe>", iframeURL.c_str());

  [ChromeEarlGrey loadURL:[self URLForHTML:iframeHTML]];
  [ChromeEarlGrey waitForWebStateFrameContainingText:"Click Me"];

  NSData* apc_data = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext page_context;
  GREYAssertTrue(
      page_context.ParseFromArray([apc_data bytes], [apc_data length]),
      @"Failed to parse PageContext");

  std::string frame_token = page_context.annotated_page_content()
                                .main_frame_data()
                                .document_identifier()
                                .serialized_token();
  FindNodeResult result =
      FindNodeWithText(page_context.annotated_page_content().root_node(),
                       "Click Me", frame_token);
  GREYAssertTrue(result.node != nullptr, @"Failed to find button node");

  std::string frameToken = result.frame_token;
  int nodeId = result.node->content_attributes().common_ancestor_dom_node_id();

  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* clickAction = action.mutable_click();
  clickAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickAction->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickAction->set_click_count(optimization_guide::proto::ClickAction::SINGLE);

  optimization_guide::proto::ActionTarget* target =
      clickAction->mutable_target();
  target->set_content_node_id(nodeId);
  target->mutable_document_identifier()->set_serialized_token(frameToken);

  [self executeAction:action];

  [ChromeEarlGrey waitForWebStateFrameContainingText:"Clicked"];
}

// Tests that the history tool successfully navigates the user back when the tab
// is on the foreground.
- (void)testHistoryBackTool_worksOnForegroundTab {
  [ChromeEarlGrey loadURL:[self URLForHTML:"PageA"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];

  [ChromeEarlGrey loadURL:[self URLForHTML:"PageB"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageB"];

  optimization_guide::proto::Action action;
  action.mutable_back()->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  [self executeAction:action];

  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];
}

// Tests that the history tool successfully navigates the user back when the tab
// is on the background.
- (void)testHistoryBackTool_worksOnBackgroundTab {
  [ChromeEarlGrey loadURL:[self URLForHTML:"PageA"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];

  [ChromeEarlGrey loadURL:[self URLForHTML:"PageB"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageB"];

  // Put the current tab on background by opening a new tab.
  NSString* backgroundTabID = [ChromeEarlGrey currentTabID];
  [ChromeEarlGrey openNewTab];
  NSString* foregroundTabID = [ChromeEarlGrey currentTabID];

  optimization_guide::proto::Action action;
  action.mutable_back()->set_tab_id(backgroundTabID.intValue);

  [self executeAction:action];

  // Verify that the browser did not change the active tab.
  GREYAssertEqualObjects(
      [ChromeEarlGrey currentTabID], foregroundTabID,
      @"Navigating the background tab changed the active tab.");

  // Switch back to the background tab to verify the navigation.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];
}

// Tests that the history tool successfully navigates the user forward.
- (void)testHistoryForwardTool {
  [ChromeEarlGrey loadURL:[self URLForHTML:"PageA"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];

  [ChromeEarlGrey loadURL:[self URLForHTML:"PageB"]];
  [ChromeEarlGrey waitForWebStateContainingText:"PageB"];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"PageA"];

  optimization_guide::proto::Action action;
  action.mutable_forward()->set_tab_id([ChromeEarlGrey currentTabID].intValue);

  [self executeAction:action];

  [ChromeEarlGrey waitForWebStateContainingText:"PageB"];
}

// Tests that the TypeTool can successfully type in an <input> given its
// coordinates.
- (void)testTypeTool_typesByCoordinates {
  const std::string inputHTML = "<input type='text'>";
  [ChromeEarlGrey loadURL:[self URLForHTML:inputHTML]];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithCSSSelector:"input"]];

  NSString* getCoordinates = [self findCenterJsForElementWithSelector:"input"];
  base::Value coordinates = [ChromeEarlGrey evaluateJavaScript:getCoordinates];
  GREYAssertTrue(coordinates.is_dict(), @"Result is not a dict");

  int x = static_cast<int>(coordinates.GetDict().FindDouble("x").value());
  int y = static_cast<int>(coordinates.GetDict().FindDouble("y").value());

  optimization_guide::proto::Action action;
  optimization_guide::proto::TypeAction* typeAction = action.mutable_type();
  typeAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  typeAction->set_text("Hello World");
  typeAction->set_mode(optimization_guide::proto::TypeAction::APPEND);

  optimization_guide::proto::ActionTarget* target =
      typeAction->mutable_target();
  target->mutable_coordinate()->set_x(x);
  target->mutable_coordinate()->set_y(y);

  [self executeAction:action];

  base::Value value = [ChromeEarlGrey
      evaluateJavaScript:@"document.querySelector('input').value"];
  GREYAssertEqualObjects(base::SysUTF8ToNSString(value.GetString()),
                         @"Hello World", @"Input value did not match");
}

// Tests that the TypeTool can successfully type in an <input> given its
// document and node identifiers.
- (void)testTypeTool_typesByIdentifiers {
  const std::string initialValue = "Initial";
  const std::string inputHTML = base::StringPrintf(
      "<input type='text' value='%s'>", initialValue.c_str());
  const std::string iframeURL = [self URLForHTML:inputHTML].spec();
  const std::string iframeHTML =
      base::StringPrintf("<iframe src='%s'></iframe>", iframeURL.c_str());

  [ChromeEarlGrey loadURL:[self URLForHTML:iframeHTML]];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithCSSSelector:"iframe"]];

  NSData* apcData = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext pageContext;
  GREYAssertTrue(pageContext.ParseFromArray([apcData bytes], [apcData length]),
                 @"Failed to parse PageContext");

  std::string mainFrameToken = pageContext.annotated_page_content()
                                   .main_frame_data()
                                   .document_identifier()
                                   .serialized_token();
  FindNodeResult result =
      FindNodeWithText(pageContext.annotated_page_content().root_node(),
                       initialValue, mainFrameToken);

  GREYAssertTrue(result.node != nullptr, @"Failed to find input node");
  int nodeId = result.node->content_attributes().common_ancestor_dom_node_id();

  optimization_guide::proto::Action action;
  optimization_guide::proto::TypeAction* typeAction = action.mutable_type();
  typeAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  typeAction->set_text("Hello World");
  typeAction->set_mode(optimization_guide::proto::TypeAction::DELETE_EXISTING);

  optimization_guide::proto::ActionTarget* target =
      typeAction->mutable_target();
  target->set_content_node_id(nodeId);
  target->mutable_document_identifier()->set_serialized_token(
      result.frame_token);

  [self executeAction:action];

  base::Value value = [ChromeEarlGrey
      evaluateJavaScript:
          @"window.frames[0].document.querySelector('input').value"];
  GREYAssertEqualObjects(base::SysUTF8ToNSString(value.GetString()),
                         @"Hello World", @"Input value did not match");
}

// Tests that the ScrollTool can successfully scroll an element given its
// coordinates.
- (void)testScrollTool_scrollsByCoordinates {
  const std::string scrollableHTML =
      R"(
      <div id="outer" style='width: 100px; height: 100px; overflow: auto;'>
        <div id="inner" style='width: 200px; height: 200px;'></div>
      </div>
      )";
  [ChromeEarlGrey loadURL:[self URLForHTML:scrollableHTML]];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithCSSSelector:"div"]];

  NSString* getCoordinates = [self findCenterJsForElementWithSelector:"#outer"];
  base::Value coordinates = [ChromeEarlGrey evaluateJavaScript:getCoordinates];
  GREYAssertTrue(coordinates.is_dict(), @"Result is not a dict");

  int x = static_cast<int>(coordinates.GetDict().FindDouble("x").value());
  int y = static_cast<int>(coordinates.GetDict().FindDouble("y").value());

  optimization_guide::proto::Action action;
  optimization_guide::proto::ScrollAction* scrollAction =
      action.mutable_scroll();
  scrollAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  scrollAction->set_direction(optimization_guide::proto::ScrollAction::DOWN);
  scrollAction->set_distance(12.999);

  optimization_guide::proto::ActionTarget* target =
      scrollAction->mutable_target();
  target->mutable_coordinate()->set_x(x);
  target->mutable_coordinate()->set_y(y);

  [self executeAction:action];

  // Safari's WkWebView rounds down the arguments provided to scrollTop and
  // scrollLeft.
  base::Value scrollTop = [ChromeEarlGrey
      evaluateJavaScript:
          @"Math.floor(document.querySelector('#outer').scrollTop).toString()"];
  GREYAssertEqualObjects(base::SysUTF8ToNSString(scrollTop.GetString()), @"12",
                         @"Element was not scrolled the expected distance.");
}

// Tests that the ScrollTool can successfully scroll an element given its
// document and node identifiers.
- (void)testScrollTool_scrollsByIdentifiers {
  const std::string scrollableHTML =
      R"(
      <div id='scroll' style='width: 100px; height: 100px; overflow: auto;'>
        Target
        <div style='width: 200px; height: 200px;'></div>
      </div>
      )";
  [ChromeEarlGrey loadURL:[self URLForHTML:scrollableHTML]];
  [ChromeEarlGrey waitForWebStateContainingText:"Target"];

  NSData* apcData = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext pageContext;
  GREYAssertTrue(pageContext.ParseFromArray([apcData bytes], [apcData length]),
                 @"Failed to parse PageContext");

  std::string mainFrameToken = pageContext.annotated_page_content()
                                   .main_frame_data()
                                   .document_identifier()
                                   .serialized_token();
  FindNodeResult result =
      FindNodeWithText(pageContext.annotated_page_content().root_node(),
                       "Target", mainFrameToken);
  GREYAssertTrue(result.node != nullptr,
                 @"Failed to find text node with \"Target\"");
  GREYAssertTrue(result.node->content_attributes().has_text_data(),
                 @"Text node does not have text data");

  // Scroll the parent node since "Target" is in a TEXT node and only ELEMENT
  // nodes are scrollable.
  int nodeId =
      result.parent->content_attributes().common_ancestor_dom_node_id();
  optimization_guide::proto::Action action;
  optimization_guide::proto::ScrollAction* scrollAction =
      action.mutable_scroll();
  scrollAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  scrollAction->set_direction(optimization_guide::proto::ScrollAction::DOWN);
  scrollAction->set_distance(12.999);

  optimization_guide::proto::ActionTarget* target =
      scrollAction->mutable_target();
  target->set_content_node_id(nodeId);
  target->mutable_document_identifier()->set_serialized_token(
      result.frame_token);

  [self executeAction:action];

  // Safari's WkWebView rounds down the arguments provided to scrollTop and
  // scrollLeft.
  base::Value scrollTop =
      [ChromeEarlGrey evaluateJavaScript:@"Math.floor(document.getElementById('"
                                         @"scroll').scrollTop).toString()"];
  GREYAssertEqualObjects(base::SysUTF8ToNSString(scrollTop.GetString()), @"12",
                         @"Element was not scrolled the expected distance.");
}

// Tests that the ScrollTool can successfully scroll the viewport when target is
// omitted.
- (void)testScrollTool_scrollsViewport {
  const std::string scrollableHTML = R"(
      <div id='big-div' style='height: 2000px;'>
      </div>
  )";
  [ChromeEarlGrey loadURL:[self URLForHTML:scrollableHTML]];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithCSSSelector:"#big-div"]];

  NSData* apcData = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext pageContext;
  GREYAssertTrue(pageContext.ParseFromArray([apcData bytes], [apcData length]),
                 @"Failed to parse PageContext");

  optimization_guide::proto::Action action;
  optimization_guide::proto::ScrollAction* scrollAction =
      action.mutable_scroll();
  scrollAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  scrollAction->set_direction(optimization_guide::proto::ScrollAction::DOWN);
  scrollAction->set_distance(123);

  [self executeAction:action];

  base::Value scrollTop = [ChromeEarlGrey
      evaluateJavaScript:@"document.scrollingElement.scrollTop.toString()"];
  GREYAssertEqualObjects(base::SysUTF8ToNSString(scrollTop.GetString()), @"123",
                         @"Viewport was not scrolled");
}
// Tests that the ScrollToTool can successfully scroll an element into view
// when given its coordinates.
- (void)testScrollToTool_scrollsByCoordinates {
  // Make the target div nearly hidden, with only its top-left corner in view.
  const std::string scrollableHTML =
      R"(
      <style>body { margin: 0; }</style>
      <div id="outer" style="width: 200px; height: 200px; overflow: auto;">
        <div id="target" style="position: relative; left: 190px; top: 190px;
                                width: 50px; height: 50px; background: red;">
        </div>
        <div id="spacer" style="height: 500px;width: 500px"></div>
      </div>
      )";
  [ChromeEarlGrey loadURL:[self URLForHTML:scrollableHTML]];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithCSSSelector:"#target"]];
  NSString* getCoordinates = base::SysUTF8ToNSString(R"(
        (function() {
          const rect = document.querySelector('#target')
                               .getBoundingClientRect();
          return {x: rect.left, y: rect.top};
        })();
      )");
  base::Value coordinates = [ChromeEarlGrey evaluateJavaScript:getCoordinates];
  GREYAssertTrue(coordinates.is_dict(), @"Result is not a dict");

  optimization_guide::proto::Action action;
  optimization_guide::proto::ScrollToAction* scrollToAction =
      action.mutable_scroll_to();
  scrollToAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  optimization_guide::proto::ActionTarget* target =
      scrollToAction->mutable_target();
  target->mutable_coordinate()->set_x(
      static_cast<int>(coordinates.GetDict().FindDouble("x").value()));
  target->mutable_coordinate()->set_y(
      coordinates.GetDict().FindDouble("y").value());

  [self executeAction:action];

  // Verify that the target is now fully within view of the outer container.
  std::string checkScroll = R"(
    (function() {
      const outer = document.getElementById('outer').getBoundingClientRect();
      const target = document.getElementById('target').getBoundingClientRect();
      const visibleX = target.left >= outer.left && target.right <= outer.right;
      const visibleY = target.top >= outer.top && target.bottom <= outer.bottom;
      return visibleX && visibleY;
    })()
    )";
  base::Value scrolled =
      [ChromeEarlGrey evaluateJavaScript:base::SysUTF8ToNSString(checkScroll)];
  GREYAssertTrue(
      scrolled.GetBool(),
      @"The target element is not fully within view of its container.");
}

// Tests that the ScrollToTool can successfully scroll an element into view
// given its document and node identifiers.
- (void)testScrollToTool_scrollsByIdentifiers {
  const std::string scrollableHTML =
      R"(
      <style>body { margin: 0; }</style>
      <div id="outer" style="width: 200px; height: 200px; overflow: auto;">
        <button id="target" style="position:relative; left:300px; width: 50px;
                                  height: 50px;">Target</button>
        <div id="spacer" style="height: 500px;width: 500px"></div>
      </div>
      )";
  [ChromeEarlGrey loadURL:[self URLForHTML:scrollableHTML]];
  [ChromeEarlGrey waitForWebStateContainingText:"Target"];

  NSData* apcData = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext pageContext;
  GREYAssertTrue(pageContext.ParseFromArray([apcData bytes], [apcData length]),
                 @"Failed to parse PageContext");
  std::string mainFrameToken = pageContext.annotated_page_content()
                                   .main_frame_data()
                                   .document_identifier()
                                   .serialized_token();
  FindNodeResult result =
      FindNodeWithText(pageContext.annotated_page_content().root_node(),
                       "Target", mainFrameToken);
  GREYAssertTrue(result.node != nullptr,
                 @"Failed to find text node with \"Target\"");
  GREYAssertTrue(result.parent != nullptr, @"Failed to find parent node");

  int nodeId =
      result.parent->content_attributes().common_ancestor_dom_node_id();
  optimization_guide::proto::Action action;
  optimization_guide::proto::ScrollToAction* scrollToAction =
      action.mutable_scroll_to();
  scrollToAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  optimization_guide::proto::ActionTarget* target =
      scrollToAction->mutable_target();
  target->set_content_node_id(nodeId);
  target->mutable_document_identifier()->set_serialized_token(
      result.frame_token);

  [self executeAction:action];

  // Verify that the target is now fully within view of the outer container.
  std::string checkScroll = R"(
    (function() {
      const outer = document.getElementById('outer').getBoundingClientRect();
      const target = document.getElementById('target').getBoundingClientRect();
      const visibleX = target.left >= outer.left && target.right <= outer.right;
      const visibleY = target.top >= outer.top && target.bottom <= outer.bottom;
      return visibleX && visibleY;
    })()
    )";
  base::Value scrolled =
      [ChromeEarlGrey evaluateJavaScript:base::SysUTF8ToNSString(checkScroll)];
  GREYAssertTrue(scrolled.GetBool(),
                 @"The target element is not within view of its container.");
}

// Tests that the SelectTool can successfully select an option in a <select>
// given its coordinates.
- (void)testSelectTool_selectsByCoordinates {
  const std::string selectHTML =
      R"(
      <select>
        <option value='v1'>Option 1</option>
        <option value='v2'>Option 2</option>
      </select>
      )";
  [ChromeEarlGrey loadURL:[self URLForHTML:selectHTML]];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithCSSSelector:"select"]];

  NSString* getCoordinates = [self findCenterJsForElementWithSelector:"select"];
  base::Value coordinates = [ChromeEarlGrey evaluateJavaScript:getCoordinates];
  GREYAssertTrue(coordinates.is_dict(), @"Result is not a dict");

  optimization_guide::proto::Action action;
  optimization_guide::proto::SelectAction* selectAction =
      action.mutable_select();
  selectAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  selectAction->set_value("v2");

  optimization_guide::proto::ActionTarget* target =
      selectAction->mutable_target();
  target->mutable_coordinate()->set_x(
      static_cast<int>(coordinates.GetDict().FindDouble("x").value()));
  target->mutable_coordinate()->set_y(
      static_cast<int>(coordinates.GetDict().FindDouble("y").value()));

  [self executeAction:action];

  bool success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        base::Value value = [ChromeEarlGrey
            evaluateJavaScript:@"document.querySelector('select').value"];
        return value.is_string() && value.GetString() == "v2";
      });
  GREYAssertTrue(success, @"Select value did not match");
}

// Tests that the SelectTool can successfully select an option in a <select>
// given its document and node identifiers.
- (void)testSelectTool_selectsByIdentifiers {
  const std::string selectHTML =
      R"(
      <select>
        <option value='v1'>Option 1</option>
        <option value='v2'>Option 2</option>
      </select>
      )";
  [ChromeEarlGrey loadURL:[self URLForHTML:selectHTML]];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithCSSSelector:"select"]];

  NSData* apcData = [ActorAppInterface fetchLatestAPC];
  optimization_guide::proto::PageContext pageContext;
  GREYAssertTrue(pageContext.ParseFromArray([apcData bytes], [apcData length]),
                 @"Failed to parse PageContext");

  std::string mainFrameToken = pageContext.annotated_page_content()
                                   .main_frame_data()
                                   .document_identifier()
                                   .serialized_token();
  FindNodeResult result = FindNodeWithPredicate(
      pageContext.annotated_page_content().root_node(),
      [](const optimization_guide::proto::ContentNode& n) {
        return n.content_attributes().has_form_control_data() &&
               n.content_attributes().form_control_data().form_control_type() ==
                   optimization_guide::proto::FormControlType::
                       FORM_CONTROL_TYPE_SELECT_ONE;
      },
      mainFrameToken);

  GREYAssertTrue(result.node != nullptr, @"Failed to find select node");
  int nodeId = result.node->content_attributes().common_ancestor_dom_node_id();

  optimization_guide::proto::Action action;
  optimization_guide::proto::SelectAction* selectAction =
      action.mutable_select();
  selectAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  selectAction->set_value("v2");
  selectAction->mutable_target()->set_content_node_id(nodeId);
  selectAction->mutable_target()
      ->mutable_document_identifier()
      ->set_serialized_token(result.frame_token);

  [self executeAction:action];

  bool success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        base::Value value = [ChromeEarlGrey
            evaluateJavaScript:@"document.querySelector('select').value"];
        return value.is_string() && value.GetString() == "v2";
      });
  GREYAssertTrue(success, @"Select value did not match");
}

@end
