// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_view_interaction_test_util.h"

#import "base/functional/bind.h"
#import "base/json/string_escape.h"
#import "base/logging.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/js_messaging/web_view_js_utils.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"
#import "ios/web/web_state/web_state_impl.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using web::NavigationManager;
using web::ValueResultFromWKResult;

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
  __block id result = nil;
  __block bool did_finish = false;
  CRWWebController* web_controller =
      WebStateImpl::FromWebState(web_state)->GetWebController();
  [web_controller executeJavaScript:base::SysUTF8ToNSString(script)
                  completionHandler:^(id handler_result, NSError*) {
                    result = handler_result;
                    did_finish = true;
                  }];

  bool completed = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_finish;
  });
  if (!completed) {
    return nullptr;
  }

  return ValueResultFromWKResult(result);
}

std::unique_ptr<base::Value> CallJavaScriptFunction(
    web::WebState* web_state,
    const std::string& function,
    const base::Value::List& parameters) {
  return CallJavaScriptFunctionForFeature(web_state, function, parameters,
                                          /*feature=*/nullptr);
}

std::unique_ptr<base::Value> CallJavaScriptFunctionForFeature(
    web::WebState* web_state,
    const std::string& function,
    const base::Value::List& parameters,
    JavaScriptFeature* feature) {
  if (!web_state) {
    DLOG(ERROR) << "JavaScript can not be called on a null WebState.";
    return nullptr;
  }

  WebFrameImpl* frame = nullptr;
  JavaScriptContentWorld* world = nullptr;
  if (feature) {
    JavaScriptFeatureManager* feature_manager =
        JavaScriptFeatureManager::FromBrowserState(
            web_state->GetBrowserState());
    world = feature_manager->GetContentWorldForFeature(feature);
    if (!world) {
      DLOG(ERROR) << "JavaScript can not be called in a null content world."
                  << "JavaScriptFeature does not appear to be configured.";
      return nullptr;
    }
    ContentWorld content_world = feature->GetSupportedContentWorld();
    frame = static_cast<WebFrameImpl*>(
        web_state->GetWebFramesManager(content_world)->GetMainWebFrame());
  } else {
    world = JavaScriptFeatureManager::GetPageContentWorldForBrowserState(
        web_state->GetBrowserState());
    frame = static_cast<WebFrameImpl*>(
        web_state->GetPageWorldWebFramesManager()->GetMainWebFrame());
  }

  if (!frame) {
    DLOG(ERROR) << "JavaScript can not be called on a null WebFrame.";
    return nullptr;
  }

  __block std::unique_ptr<base::Value> result;
  __block bool did_finish = false;
  bool function_call_successful = frame->CallJavaScriptFunctionInContentWorld(
      function, parameters, world, base::BindOnce(^(const base::Value* value) {
        if (value)
          result = std::make_unique<base::Value>(value->Clone());
        did_finish = true;
      }),
      kWaitForJSCompletionTimeout);

  if (!function_call_successful) {
    DLOG(ERROR) << "JavaScript failed to be called on WebFrame.";
    return nullptr;
  }

  // Wait twice as long as the completion block above should always be called at
  // the timeout time per WebFrame API contract.
  bool completed =
      WaitUntilConditionOrTimeout(2 * kWaitForJSCompletionTimeout, ^{
        return did_finish;
      });
  if (!completed) {
    DLOG(ERROR) << "Expected callback was never called.";
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
  // TODO(crbug.com/40652803): Replace delay with improved JavaScript.
  // As of iOS 13.1, devices need additional time to stabalize the page before
  // getting the element location. Without this wait, the element's bounding
  // rect will be incorrect.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));
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

  __block std::unique_ptr<base::Value::Dict> rect;

  bool found = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    std::unique_ptr<base::Value> value =
        ExecuteJavaScript(web_state, kGetBoundsScript);
    if (base::Value::Dict* dictionary = value->GetIfDict()) {
      if (const std::string* error = dictionary->FindString("error")) {
        DLOG(ERROR) << "Error getting rect: " << error << ", retrying..";
      } else {
        rect = std::make_unique<base::Value::Dict>(dictionary->Clone());
        return true;
      }
    }
    return false;
  });

  if (!found)
    return CGRectNull;

  std::optional<double> left = rect->FindDouble("left");
  std::optional<double> top = rect->FindDouble("top");
  std::optional<double> width = rect->FindDouble("width");
  std::optional<double> height = rect->FindDouble("height");
  if (!(left && top && width && height))
    return CGRectNull;

  CGFloat scale = [[web_state->GetWebViewProxy() scrollViewProxy] zoomScale];

  CGRect elementFrame =
      CGRectMake(left.value() * scale, top.value() * scale,
                 width.value() * scale, height.value() * scale);
  UIEdgeInsets contentInset =
      web_state->GetWebViewProxy().scrollViewProxy.contentInset;
  elementFrame =
      CGRectOffset(elementFrame, contentInset.left, contentInset.top);

  return elementFrame;
}

// Returns whether the Javascript action specified by `action` ran on the
// element retrieved by the Javascript snippet `element_script` in the passed
// `web_state`. `error` can be nil, and will return any error from executing
// JavaScript.
bool RunActionOnWebViewElementWithScript(web::WebState* web_state,
                                         const std::string& element_script,
                                         ElementAction action,
                                         NSError* __autoreleasing* error) {
  CRWWebController* web_controller =
      WebStateImpl::FromWebState(web_state)->GetWebController();
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

  // `executeUserJavaScript:completionHandler:` is no-op for app-specific URLs,
  // so simulate a user gesture by calling TouchTracking method.
  [web_controller touched:YES];
  [web_controller executeJavaScript:script
                  completionHandler:^(id result, NSError* innerError) {
                    did_complete = true;
                    element_found = [result boolValue];
                    block_error = [innerError copy];
                  }];

  bool js_finished = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_complete;
  });

  if (error) {
    *error = block_error;
  }

  return js_finished && element_found;
}

// Returns whether the Javascript action specified by `action` ran on
// `element_id` in the passed `web_state`. `error` can be nil, and will return
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
