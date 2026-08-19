// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/test/actor_tools_base_test_case.h"

#import <memory>
#import <string>

#import "base/base_paths.h"
#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/strings/escape.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
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

}  // namespace

FindNodeResult FindNodeWithPredicate(
    const optimization_guide::proto::ContentNode& node,
    base::FunctionRef<bool(const optimization_guide::proto::ContentNode&)>
        predicate,
    const std::string& current_frame_token,
    const optimization_guide::proto::ContentNode* parent) {
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
  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
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
    const optimization_guide::proto::ContentNode* parent) {
  return FindNodeWithPredicate(
      node,
      [&text](const optimization_guide::proto::ContentNode& n) {
        if (n.content_attributes().has_text_data()) {
          std::string original =
              n.content_attributes().text_data().text_content();
          std::string trimmed = base::CollapseWhitespaceASCII(
              original, /*trim_sequences_with_line_breaks=*/true);
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

@implementation ActorToolsBaseTestCase {
  std::unique_ptr<net::test_server::EmbeddedTestServer> _crossOriginServer;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  base::test::FeatureRefAndParams actorToolsConfig(
      kActorTools,
      {
          {"PageStabilityEnabled", "true"},
          {"ActorPageStabilityMutationCap",
           base::StringPrintf("%d", kPageStabilityMutationCap)},
          {"ActorPageStabilityWindowDuration",
           base::StringPrintf("%dms", kPageStabilityWindowDurationMs)},
          {"ActorPageStabilityTimeout",
           base::StringPrintf("%dms", kPageStabilityTimeoutMs)},
          {"ActorPageStabilityMinWait",
           base::StringPrintf("%dms", kPageStabilityMinWaitMs)},
          {"ActorPageStabilityLcpDelay",
           base::StringPrintf("%dms", kPageStabilityLcpDelayMs)},
          {"ActorPageStabilityAutofillPredictionsTimeout",
           base::StringPrintf("%dms",
                              kPageStabilityAutofillPredictionsTimeoutMs)},
      });

  config.features_enabled_and_params.push_back(actorToolsConfig);
  config.features_enabled.push_back(
      autofill::features::kAutofillDelayApcForPredictions);
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

- (net::test_server::EmbeddedTestServer*)crossOriginServer {
  return _crossOriginServer.get();
}

#pragma mark - Helpers

- (GURL)URLForHTML:(const std::string&)html
            server:(net::test_server::EmbeddedTestServer*)server {
  GURL url = server->GetURL("/echo");
  std::string escapedContent = base::EscapeQueryParamValue(html, true);
  GURL::Replacements replacements;
  std::string query = "content=" + escapedContent;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

- (GURL)URLForHTML:(const std::string&)html {
  return [self URLForHTML:html server:self.testServer];
}

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

- (void)setCoordinatesOnTarget:(optimization_guide::proto::ActionTarget*)target
                  withSelector:(const std::string&)selector {
  NSString* script = [self findCenterJsForElementWithSelector:selector];
  base::Value result = [ChromeEarlGrey evaluateJavaScript:script];
  GREYAssertTrue(result.is_dict(), @"Result is not a dict");

  std::optional<double> optionalX = result.GetDict().FindDouble("x");
  GREYAssertTrue(optionalX.has_value(), @"x coordinate not found");
  int x = static_cast<int>(optionalX.value());

  std::optional<double> optionalY = result.GetDict().FindDouble("y");
  GREYAssertTrue(optionalY.has_value(), @"y coordinate not found");
  int y = static_cast<int>(optionalY.value());

  target->mutable_coordinate()->set_x(x);
  target->mutable_coordinate()->set_y(y);
}

- (NSError*)executeAction:(const optimization_guide::proto::Action&)action
    [[nodiscard]] {
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
  return executionError;
}

- (void)executeActions:(const optimization_guide::proto::Actions&)actions {
  std::string serializedActions;
  actions.SerializeToString(&serializedActions);
  NSData* actionsData = [NSData dataWithBytes:serializedActions.data()
                                       length:serializedActions.length()];

  __block NSError* executionError = nil;
  __block BOOL actionsCompleted = NO;

  [ActorAppInterface executeActionsWithProto:actionsData
                                  completion:^(NSError* error) {
                                    executionError = error;
                                    actionsCompleted = YES;
                                  }];

  BOOL success =
      [[GREYCondition conditionWithName:@"Wait for actions completion"
                                  block:^BOOL {
                                    return actionsCompleted;
                                  }] waitWithTimeout:10.0];

  GREYAssertTrue(success, @"Actions timed out.");
  GREYAssertNil(executionError, @"Actions failed: %@", executionError);
}

@end
