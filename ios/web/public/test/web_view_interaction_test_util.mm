// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_view_interaction_test_util.h"

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/crw_js_injector.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::NavigationManager;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

namespace web {
namespace test {

enum ElementAction {
  ELEMENT_ACTION_CLICK,
  ELEMENT_ACTION_FOCUS,
  ELEMENT_ACTION_SUBMIT,
  ELEMENT_ACTION_SELECT,
};

std::unique_ptr<base::Value> ExecuteJavaScript(web::WebState* web_state,
                                               const std::string& script) {
  __block std::unique_ptr<base::Value> result;
  __block bool did_finish = false;
  web_state->ExecuteJavaScript(base::UTF8ToUTF16(script),
                               base::BindOnce(^(const base::Value* value) {
                                 if (value)
                                   result = value->CreateDeepCopy();
                                 did_finish = true;
                               }));

  bool completed = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_finish;
  });
  if (!completed) {
    return nullptr;
  }

  // As result is marked __block, this return call does a copy and not a move
  // (marking the variable as __block mean it is allocated in the block object
  // and not the stack). Use an explicit move to a local variable.
  //
  // Fixes the following compilation failures:
  //   ../web_view_interaction_test_util.mm:58:10: error:
  //       call to implicitly-deleted copy constructor of
  //       'std::unique_ptr<base::Value>'
  //
  //   ../web_view_interaction_test_util.mm:58:10: error:
  //       moving a local object in a return statement prevents copy elision
  //       [-Werror,-Wpessimizing-move]
  std::unique_ptr<base::Value> stack_result = std::move(result);
  return stack_result;
}

CGRect GetBoundingRectOfElement(web::WebState* web_state,
                                ElementSelector* selector) {
#if !TARGET_IPHONE_SIMULATOR
  // TODO(crbug.com/1013714): Replace delay with improved JavaScript.
  // As of iOS 13.1, devices need additional time to stabalize the page before
  // getting the element location. Without this wait, the element's bounding
  // rect will be incorrect.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.5));
#endif

  std::string selector_script =
      base::SysNSStringToUTF8(selector.selectorScript);
  std::string selector_description =
      base::SysNSStringToUTF8(selector.selectorDescription);
  std::string quoted_description;
  bool success = base::EscapeJSONString(
      selector_description, true /* put_in_quotes */, &quoted_description);
  if (!success) {
    DLOG(ERROR) << "Error quoting description: "
                << selector.selectorDescription;
  }

  std::string kGetBoundsScript =
      "(function() {"
      "  var element = " +
      selector_script +
      ";"
      "  if (!element) {"
      "    var description = " +
      quoted_description +
      ";"
      "    return {'error': 'Element ' + description + ' not found'};"
      "  }"
      "  var rect = element.getBoundingClientRect();"
      "  return {"
      "      'left': rect.left,"
      "      'top': rect.top,"
      "      'width': rect.right - rect.left,"
      "      'height': rect.bottom - rect.top,"
      "    };"
      "})();";

  __block base::DictionaryValue const* rect = nullptr;

  bool found = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    std::unique_ptr<base::Value> value =
        ExecuteJavaScript(web_state, kGetBoundsScript);
    base::DictionaryValue* dictionary = nullptr;
    if (value && value->GetAsDictionary(&dictionary)) {
      std::string error;
      if (dictionary->GetString("error", &error)) {
        DLOG(ERROR) << "Error getting rect: " << error << ", retrying..";
      } else {
        rect = dictionary->DeepCopy();
        return true;
      }
    }
    return false;
  });

  if (!found)
    return CGRectNull;

  double left, top, width, height;
  if (!(rect->GetDouble("left", &left) && rect->GetDouble("top", &top) &&
        rect->GetDouble("width", &width) &&
        rect->GetDouble("height", &height))) {
    return CGRectNull;
  }

  CGFloat scale = [[web_state->GetWebViewProxy() scrollViewProxy] zoomScale];

  CGRect elementFrame =
      CGRectMake(left * scale, top * scale, width * scale, height * scale);
  UIEdgeInsets contentInset =
      web_state->GetWebViewProxy().scrollViewProxy.contentInset;
  elementFrame =
      CGRectOffset(elementFrame, contentInset.left, contentInset.top);

  return elementFrame;
}

// Returns whether the Javascript action specified by |action| ran on the
// element retrieved by the Javascript snippet |element_script| in the passed
// |web_state|. |error| can be nil, and will return any error from executing
// JavaScript.
bool RunActionOnWebViewElementWithScript(web::WebState* web_state,
                                         const std::string& element_script,
                                         ElementAction action,
                                         NSError* __autoreleasing* error) {
  CRWWebController* web_controller =
      static_cast<WebStateImpl*>(web_state)->GetWebController();
  const char* js_action = nullptr;
  switch (action) {
    case ELEMENT_ACTION_CLICK:
      js_action = ".click()";
      break;
    case ELEMENT_ACTION_FOCUS:
      js_action = ".focus();";
      break;
    case ELEMENT_ACTION_SUBMIT:
      js_action = ".submit();";
      break;
    case ELEMENT_ACTION_SELECT:
      js_action = ".selected = true;";
      break;
  }
  NSString* script = [NSString stringWithFormat:
                                   @"(function() {"
                                    "  var element = %s;"
                                    "  if (element) {"
                                    "    element%s;"
                                    "    return true;"
                                    "  }"
                                    "  return false;"
                                    "})();",
                                   element_script.c_str(), js_action];
  __block bool did_complete = false;
  __block bool element_found = false;
  __block NSError* block_error = nil;

  // |executeUserJavaScript:completionHandler:| is no-op for app-specific URLs,
  // so simulate a user gesture by calling TouchTracking method.
  [web_controller touched:YES];
  [web_controller.jsInjector executeJavaScript:script
                             completionHandler:^(id result, NSError* error) {
                               did_complete = true;
                               element_found = [result boolValue];
                               block_error = [error copy];
                             }];

  bool js_finished = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_complete;
  });

  if (error) {
    *error = block_error;
  }

  return js_finished && element_found;
}

// Returns whether the Javascript action specified by |action| ran on
// |element_id| in the passed |web_state|. |error| can be nil, and will return
// any error from executing JavaScript.
bool RunActionOnWebViewElementWithId(web::WebState* web_state,
                                     const std::string& element_id,
                                     ElementAction action,
                                     NSError* __autoreleasing* error) {
  std::string element_script =
      base::StringPrintf("document.getElementById('%s')", element_id.c_str());
  return RunActionOnWebViewElementWithScript(web_state, element_script, action,
                                             error);
}

bool TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id) {
  return RunActionOnWebViewElementWithId(web_state, element_id,
                                         ELEMENT_ACTION_CLICK, nil);
}

bool TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id,
                             NSError* __autoreleasing* error) {
  return RunActionOnWebViewElementWithId(web_state, element_id,
                                         ELEMENT_ACTION_CLICK, error);
}

bool TapWebViewElementWithIdInIframe(web::WebState* web_state,
                                     const std::string& element_id) {
  std::string element_script = base::StringPrintf(
      "window.frames[0].document.getElementById('%s')", element_id.c_str());
  return RunActionOnWebViewElementWithScript(web_state, element_script,
                                             ELEMENT_ACTION_CLICK, nil);
}

bool FocusWebViewElementWithId(web::WebState* web_state,
                               const std::string& element_id) {
  return RunActionOnWebViewElementWithId(web_state, element_id,
                                         ELEMENT_ACTION_FOCUS, nil);
}

bool SubmitWebViewFormWithId(web::WebState* web_state,
                             const std::string& form_id) {
  return RunActionOnWebViewElementWithId(web_state, form_id,
                                         ELEMENT_ACTION_SUBMIT, nil);
}

bool SelectWebViewElementWithId(web::WebState* web_state,
                                const std::string& element_id) {
  return RunActionOnWebViewElementWithId(web_state, element_id,
                                         ELEMENT_ACTION_SELECT, nil);
}

}  // namespace test
}  // namespace web
