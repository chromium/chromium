// Copyright 2017 The Chromium Authors. All rights reserved.
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

// Returns true if there is a web view for |web_state| that contains |text|.
// Otherwise, returns false.
bool IsWebViewContainingText(web::WebState* web_state, const std::string& text);

// Waits for the given web state to contain |text|. If the condition is not met
// within |timeout| false is returned.
bool WaitForWebViewContainingText(
    web::WebState* web_state,
    std::string text,
    NSTimeInterval timeout = base::test::ios::kWaitForPageLoadTimeout)
    WARN_UNUSED_RESULT;

// Waits for the given web state to not contain |text|. If the condition is not
// met within |timeout| false is returned.
bool WaitForWebViewNotContainingText(
    web::WebState* web_state,
    std::string text,
    NSTimeInterval timeout = base::test::ios::kWaitForPageLoadTimeout)
    WARN_UNUSED_RESULT;

// Waits for a web view with the corresponding |image_id| and |image_state|, in
// the given |web_state|.
bool WaitForWebViewContainingImage(std::string image_id,
                                   web::WebState* web_state,
                                   ImageStateElement image_state);

// Returns true if there is a web view for |web_state| that contains an
// element for the |selector|.
bool IsWebViewContainingElement(web::WebState* web_state,
                                ElementSelector* selector);

// Waits for |web_state| to contain an element for |selector|.
bool WaitForWebViewContainingElement(web::WebState* web_state,
                                     ElementSelector* selector)
    WARN_UNUSED_RESULT;

// Waits for |web_state| to not contain an element for |selector|.
bool WaitForWebViewNotContainingElement(web::WebState* web_state,
                                        ElementSelector* selector)
    WARN_UNUSED_RESULT;

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_VIEW_CONTENT_TEST_UTIL_H_
