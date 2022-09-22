// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_VIEW_CONTENT_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_WEB_VIEW_CONTENT_TEST_UTIL_H_

#import "base/test/ios/wait_util.h"
#include "ios/web/public/test/element_selector.h"

namespace web {
class WebState;

namespace test {

// Enum describing loaded/blocked state of an image html element.
enum ImageStateElement {
  // Element was not loaded by WebState.
  IMAGE_STATE_BLOCKED = 1,
  // Element was fullt loaded by WebState.
  IMAGE_STATE_LOADED,
};

// Returns true if there is a web view for `web_state` that contains `text`.
// Otherwise, returns false.
bool IsWebViewContainingText(web::WebState* web_state, const std::string& text);

// Returns true if there is a frame from `web_state` that contains `text`.
// This method waits for the JavaScript message response.
// `FindInPageJavaScriptFeature` must be configured for `web_state` in order for
// this function to correctly return results.
bool IsWebViewContainingTextInFrame(web::WebState* web_state,
                                    const std::string& text);

// Waits for the given web state to contain `text`. If the condition is not met
// within `timeout` false is returned.
[[nodiscard]] bool WaitForWebViewContainingText(
    web::WebState* web_state,
    std::string text,
    base::TimeDelta timeout = base::test::ios::kWaitForPageLoadTimeout);

// Waits for the given web state to not contain `text`. If the condition is not
// met within `timeout` false is returned.
[[nodiscard]] bool WaitForWebViewNotContainingText(
    web::WebState* web_state,
    std::string text,
    base::TimeDelta timeout = base::test::ios::kWaitForPageLoadTimeout);

// Waits for the given web state to have a frame that contains `text`. If the
// condition is not met within `timeout` false is returned.
// `FindInPageJavaScriptFeature` must be configured for `web_state` in order for
// this function to correctly return results.
[[nodiscard]] bool WaitForWebViewContainingTextInFrame(
    web::WebState* web_state,
    std::string text,
    base::TimeDelta timeout = base::test::ios::kWaitForPageLoadTimeout);

// Waits for a web view with the corresponding `image_id` and `image_state`, in
// the given `web_state`.
bool WaitForWebViewContainingImage(std::string image_id,
                                   web::WebState* web_state,
                                   ImageStateElement image_state);

// Returns true if there is a web view for `web_state` that contains an
// element for the `selector`.
bool IsWebViewContainingElement(web::WebState* web_state,
                                ElementSelector* selector);

// Waits for `web_state` to contain an element for `selector`.
[[nodiscard]] bool WaitForWebViewContainingElement(web::WebState* web_state,
                                                   ElementSelector* selector);

// Waits for `web_state` to not contain an element for `selector`.
[[nodiscard]] bool WaitForWebViewNotContainingElement(
    web::WebState* web_state,
    ElementSelector* selector);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_VIEW_CONTENT_TEST_UTIL_H_
