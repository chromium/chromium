// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_TEST_ACTOR_TOOLS_BASE_TEST_CASE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_TEST_ACTOR_TOOLS_BASE_TEST_CASE_H_

#import <string>

#import "base/functional/function_ref.h"
#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

inline constexpr int kPageStabilityMutationCap = 2;
inline constexpr int kPageStabilityWindowDurationMs = 100;
inline constexpr int kPageStabilityTimeoutMs = 2000;
inline constexpr int kPageStabilityMinWaitMs = 1000;
inline constexpr int kPageStabilityLcpDelayMs = 3000;
inline constexpr int kPageStabilityAutofillPredictionsTimeoutMs = 3000;

// Result of searching for a content node within PageContext.
struct FindNodeResult {
  raw_ptr<const optimization_guide::proto::ContentNode> node = nullptr;
  raw_ptr<const optimization_guide::proto::ContentNode> parent = nullptr;
  std::string frame_token;
};

// Recursively finds a node matching `predicate` under `node`.
FindNodeResult FindNodeWithPredicate(
    const optimization_guide::proto::ContentNode& node,
    base::FunctionRef<bool(const optimization_guide::proto::ContentNode&)>
        predicate,
    const std::string& current_frame_token,
    const optimization_guide::proto::ContentNode* parent = nullptr);

// Finds a node containing `text` under `node`.
FindNodeResult FindNodeWithText(
    const optimization_guide::proto::ContentNode& node,
    const std::string& text,
    const std::string& current_frame_token,
    const optimization_guide::proto::ContentNode* parent = nullptr);

// Base test case for Actor tools EarlGrey tests.
@interface ActorToolsBaseTestCase : ChromeTestCase

// Returns the embedded test server configured for cross-origin requests.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* crossOriginServer;

// Returns a URL for the given `html` content using `server`.
- (GURL)URLForHTML:(const std::string&)html
            server:(net::test_server::EmbeddedTestServer*)server;

// Returns a URL for the given `html` content using the embedded test server.
- (GURL)URLForHTML:(const std::string&)html;

// Returns a JavaScript function that finds the center coordinates of the
// element matching `selector`.
- (NSString*)findCenterJsForElementWithSelector:(const std::string&)selector;

// Sets the coordinates on `target` using the center coordinates of the element
// matching `selector`.
- (void)setCoordinatesOnTarget:(optimization_guide::proto::ActionTarget*)target
                  withSelector:(const std::string&)selector;

// Executes the given `action` and returns the error if execution failed, or nil
// on success.
- (NSError*)executeAction:(const optimization_guide::proto::Action&)action
    [[nodiscard]];

// Executes the given `actions` and waits for completion.
- (void)executeActions:(const optimization_guide::proto::Actions&)actions;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_TEST_ACTOR_TOOLS_BASE_TEST_CASE_H_
