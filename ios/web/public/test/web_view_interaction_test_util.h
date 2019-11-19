// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_VIEW_INTERACTION_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_WEB_VIEW_INTERACTION_TEST_UTIL_H_

#import <UIKit/UIKit.h>

#include <string>

#import "base/ios/block_types.h"
#include "base/values.h"

#include "ios/web/public/test/element_selector.h"

namespace web {
class WebState;

namespace test {

// Synchronously returns the result of executed JavaScript, returning nullptr
// if the JavaScript does not complete.
std::unique_ptr<base::Value> ExecuteJavaScript(web::WebState* web_state,
                                               const std::string& script);

// Returns the CGRect, in the coordinate system of web_state's view, that
// encloses the element returned by |selector| in |web_state|'s webview.
// There is no guarantee that the CGRect returned is inside the current window;
// callers should check and act accordingly (scrolling the webview, perhaps).
// Returns CGRectNull if no element could be found.
CGRect GetBoundingRectOfElement(web::WebState* web_state,
                                ElementSelector* selector);

// Returns whether the element with |element_id| in the passed |web_state| has
// been tapped using a JavaScript click() event.
bool TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id);

// Returns whether the element with |element_id| in the passed |web_state| has
// been tapped using a JavaScript click() event. |error| can be nil.
bool TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id,
                             NSError* __autoreleasing* error);

// Looks for an element with |element_id| within window.frames[0] in the passed
// |web_state|. Returns whether this element has been tapped using a JavaScript
// click() event. This only works on same-origin iframes.
bool TapWebViewElementWithIdInIframe(web::WebState* web_state,
                                     const std::string& element_id);

// Returns whether the element with |element_id| in the passed |web_state| has
// been focused using a JavaScript focus() event.
bool FocusWebViewElementWithId(web::WebState* web_state,
                               const std::string& element_id);

// Returns whether the form with |form_id| in the passed |web_state| has been
// submitted using a JavaScript submit() event.
bool SubmitWebViewFormWithId(web::WebState* web_state,
                             const std::string& form_id);

// Returns whether the <option id="|element_id|"> in the passed |web_state| has
// been selected using a JavaScript "selected=true" operation.
bool SelectWebViewElementWithId(web::WebState* web_state,
                                const std::string& element_id);
}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_VIEW_INTERACTION_TEST_UTIL_H_
