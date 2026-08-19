// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_tools_base_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "url/gurl.h"

@interface ActorWebToolsTestCase : ActorToolsBaseTestCase
@end

@implementation ActorWebToolsTestCase

#pragma mark - ClickTool Tests

// Tests that the click tool successfully clicks an element using its
// coordinates.
- (void)testClickTool_clicksByCoordinates {
  const std::string buttonHTML =
      "<button onclick='this.innerText=\"Clicked\"'>Click Me</button>";

  [ChromeEarlGrey loadURL:[self URLForHTML:buttonHTML]];
  [ChromeEarlGrey waitForWebStateContainingText:"Click Me"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* clickAction = action.mutable_click();
  clickAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickAction->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickAction->set_click_count(optimization_guide::proto::ClickAction::SINGLE);

  [self setCoordinatesOnTarget:clickAction->mutable_target()
                  withSelector:"button"];

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

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
  std::string frameToken = result.frame_token;
  int nodeId = result.node->content_attributes().common_ancestor_dom_node_id();

  GREYAssertFalse(frameToken.empty(), @"Failed to get frame token.");
  GREYAssertTrue(nodeId > 0, @"Failed to get node ID.");

  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* clickAction = action.mutable_click();
  clickAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickAction->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickAction->set_click_count(optimization_guide::proto::ClickAction::SINGLE);

  optimization_guide::proto::ActionTarget* target =
      clickAction->mutable_target();
  target->set_content_node_id(nodeId);
  target->mutable_document_identifier()->set_serialized_token(frameToken);

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  [ChromeEarlGrey waitForWebStateFrameContainingText:"Clicked"];
}

// Tests that the click tool successfully clicks an element inside a
// cross-origin iframe.
- (void)testClickTool_worksOnCrossOriginIframe {
  const std::string buttonHTML =
      "<button onclick='this.innerText=\"Clicked\"'>Click Me</button>";
  const std::string iframeURL =
      [self URLForHTML:buttonHTML server:self.crossOriginServer].spec();
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

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  [ChromeEarlGrey waitForWebStateFrameContainingText:"Clicked"];
}

// Tests that a Click action that triggers navigation can be followed by a Wait
// action in a single task without causing a double-teardown crash.
// This is a regression test for crbug.com/532117001.
- (void)testClickTool_navigationFollowedByWait_doesNotCrash {
  GURL destinationURL = [self URLForHTML:"Hello"];
  std::string linkHTML = base::StringPrintf(
      "<a id='link' href='%s'>Go to B</a>", destinationURL.spec().c_str());
  [ChromeEarlGrey loadURL:[self URLForHTML:linkHTML]];
  [ChromeEarlGrey waitForWebStateContainingText:"Go to B"];

  // Create the Click and Wait actions.
  base::Value linkCoordinates = [ChromeEarlGrey
      evaluateJavaScript:[self findCenterJsForElementWithSelector:"#link"]];
  GREYAssertTrue(linkCoordinates.is_dict(), @"Link coordinates is not a dict");
  int x = static_cast<int>(linkCoordinates.GetDict().FindDouble("x").value());
  int y = static_cast<int>(linkCoordinates.GetDict().FindDouble("y").value());

  optimization_guide::proto::Actions actions;

  optimization_guide::proto::Action* clickLinkAction = actions.add_actions();
  optimization_guide::proto::ClickAction* clickLink =
      clickLinkAction->mutable_click();
  clickLink->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickLink->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickLink->set_click_count(optimization_guide::proto::ClickAction::SINGLE);
  clickLink->mutable_target()->mutable_coordinate()->set_x(x);
  clickLink->mutable_target()->mutable_coordinate()->set_y(y);

  optimization_guide::proto::Action* waitAction = actions.add_actions();
  waitAction->mutable_wait()->set_wait_time_ms(500);

  // Execute both actions in a single task. Under the hood, this creates and
  // destructs of two ToolController objects. This verifies that the lifetimes
  // are managed correctly and don't cause a crash.
  [self executeActions:actions];
}

#pragma mark - TypeTool Tests

// Tests that the TypeTool can successfully type in an <input> given its
// coordinates.
- (void)testTypeTool_typesByCoordinates {
  const std::string inputHTML = "<input type='text'>";
  [ChromeEarlGrey loadURL:[self URLForHTML:inputHTML]];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithCSSSelector:"input"]];

  optimization_guide::proto::Action action;
  optimization_guide::proto::TypeAction* typeAction = action.mutable_type();
  typeAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  typeAction->set_text("Hello World");
  typeAction->set_mode(optimization_guide::proto::TypeAction::APPEND);

  [self setCoordinatesOnTarget:typeAction->mutable_target()
                  withSelector:"input"];

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

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

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  base::Value value = [ChromeEarlGrey
      evaluateJavaScript:
          @"window.frames[0].document.querySelector('input').value"];
  GREYAssertEqualObjects(base::SysUTF8ToNSString(value.GetString()),
                         @"Hello World", @"Input value did not match");
}

#pragma mark - ScrollTool Tests

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

  optimization_guide::proto::Action action;
  optimization_guide::proto::ScrollAction* scrollAction =
      action.mutable_scroll();
  scrollAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  scrollAction->set_direction(optimization_guide::proto::ScrollAction::DOWN);
  scrollAction->set_distance(12.999);

  [self setCoordinatesOnTarget:scrollAction->mutable_target()
                  withSelector:"#outer"];

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

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

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

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

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  base::Value scrollTop = [ChromeEarlGrey
      evaluateJavaScript:@"document.scrollingElement.scrollTop.toString()"];
  GREYAssertEqualObjects(base::SysUTF8ToNSString(scrollTop.GetString()), @"123",
                         @"Viewport was not scrolled");
}

#pragma mark - ScrollToTool Tests

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
      static_cast<int>(coordinates.GetDict().FindDouble("y").value()));

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

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

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

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

#pragma mark - SelectTool Tests

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

  optimization_guide::proto::Action action;
  optimization_guide::proto::SelectAction* selectAction =
      action.mutable_select();
  selectAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  selectAction->set_value("v2");

  [self setCoordinatesOnTarget:selectAction->mutable_target()
                  withSelector:"select"];

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

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

  GREYAssertNil([self executeAction:action], @"Action execution failed.");

  bool success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        base::Value value = [ChromeEarlGrey
            evaluateJavaScript:@"document.querySelector('select').value"];
        return value.is_string() && value.GetString() == "v2";
      });
  GREYAssertTrue(success, @"Select value did not match");
}

@end
