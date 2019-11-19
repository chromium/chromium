// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/web_view_actions.h"

#import <WebKit/WebKit.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::test::ExecuteJavaScript;

namespace {

// Long press duration to trigger context menu.
const NSTimeInterval kContextMenuLongPressDuration = 1.0;

// Duration to wait for verification of JavaScript action.
// TODO(crbug.com/670910): Reduce duration if the time required for verification
// is reduced on devices.
const NSTimeInterval kWaitForVerificationTimeout = 8.0;

// Generic verification injector. Injects one-time mousedown verification into
// |web_state| that will set the boolean pointed to by |verified| to true when
// |web_state|'s webview registers the mousedown event.
std::unique_ptr<web::WebState::ScriptCommandSubscription>
AddVerifierToElementWithPrefix(web::WebState* web_state,
                               ElementSelector* selector,
                               const std::string& prefix,
                               bool* verified) {
  const char kCallbackCommand[] = "verified";
  const std::string kCallbackInvocation = prefix + '.' + kCallbackCommand;

  const char kAddInteractionVerifierScriptTemplate[] =
      "(function() {"
      // First template param: element.
      "  var element = %1$s;"
      "  if (!element)"
      "    return 'Element not found';"
      "  var invokeType = typeof __gCrWeb.message;"
      "  if (invokeType != 'object')"
      "    return 'Host invocation not installed (' + invokeType + ')';"
      "  var options = {'capture' : true, 'once' : true, 'passive' : true};"
      "  element.addEventListener('mousedown', function(event) {"
      "      __gCrWeb.message.invokeOnHost("
      // Second template param: callback command.
      "          {'command' : '%2$s' });"
      "  }, options);"
      "  return true;"
      "})();";

  std::string selector_script =
      base::SysNSStringToUTF8(selector.selectorScript);
  const std::string kAddVerifierScript =
      base::StringPrintf(kAddInteractionVerifierScriptTemplate,
                         selector_script.c_str(), kCallbackInvocation.c_str());

  bool success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        bool verifier_added = false;
        std::unique_ptr<base::Value> value =
            web::test::ExecuteJavaScript(web_state, kAddVerifierScript);
        if (value) {
          std::string error;
          if (value->GetAsString(&error)) {
            DLOG(ERROR) << "Verifier injection failed: " << error
                        << ", retrying.";
          } else if (value->GetAsBoolean(&verifier_added)) {
            return true;
          }
        }
        return false;
      });

  if (!success)
    return nullptr;

  // The callback doesn't care about any of the parameters, just whether it is
  // called or not.
  auto callback = base::BindRepeating(
      ^(const base::DictionaryValue& /* json */, const GURL& /* origin_url */,
        bool /* user_is_interacting */, web::WebFrame* /* sender_frame */) {
        *verified = true;
      });

  return web_state->AddScriptCommandCallback(callback, prefix);
}

// Returns a no element found error.
id<GREYAction> WebViewElementNotFound(ElementSelector* selector) {
  NSString* description = [NSString
      stringWithFormat:@"Couldn't locate a bounding rect for element %@; "
                       @"either it isn't there or it has no area.",
                       selector.selectorDescription];
  GREYPerformBlock throw_error =
      ^BOOL(id /* element */, __strong NSError** error) {
        NSDictionary* user_info = @{NSLocalizedDescriptionKey : description};
        *error = [NSError errorWithDomain:kGREYInteractionErrorDomain
                                     code:kGREYInteractionActionFailedErrorCode
                                 userInfo:user_info];
        return NO;
      };
  return [GREYActionBlock actionWithName:@"Locate element bounds"
                            performBlock:throw_error];
}

// Checks that a rectangle in a view (expressed in this view's coordinate
// system) is actually visible and potentially tappable.
bool IsRectVisibleInView(CGRect rect, UIView* view) {
  // Take a point at the center of the element.
  CGPoint point_in_view = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));

  // Converts its coordinates to window coordinates.
  CGPoint point_in_window =
      [view convertPoint:point_in_view toView:view.window];

  // Check if this point is actually on screen.
  if (!CGRectContainsPoint(view.window.frame, point_in_window)) {
    return false;
  }

  // Check that the view is not covered by another view).
  UIView* hit = [view.window hitTest:point_in_window withEvent:nil];
  while (hit) {
    if (hit == view) {
      return true;
    }
    hit = hit.superview;
  }
  return false;
}

}  // namespace

namespace web {

id<GREYAction> WebViewVerifiedActionOnElement(WebState* state,
                                              id<GREYAction> action,
                                              ElementSelector* selector) {
  NSString* action_name =
      [NSString stringWithFormat:@"Verified action (%@) on webview element %@.",
                                 action.name, selector.selectorDescription];
  const std::string prefix =
      base::StringPrintf("__web_test_%p_interaction", &selector);

  GREYPerformBlock verified_tap = ^BOOL(id element, __strong NSError** error) {
    // A pointer to |verified| is passed into AddVerifierToElementWithPrefix()
    // so the verifier can update its value, but |verified| also needs to be
    // marked as __block so that waitUntilCondition(), below, can access it by
    // reference.
    __block bool verified = false;

    __block std::unique_ptr<web::WebState::ScriptCommandSubscription>
        subscription;
    // GREYPerformBlock executes on background thread by default in EG2.
    // Dispatch any call involving UI API to UI thread as they can't be executed
    // on background thread. See go/eg2-migration#greyactions-threading-behavior
    grey_dispatch_sync_on_main_thread(^{
      // Inject the verifier.
      subscription =
          AddVerifierToElementWithPrefix(state, selector, prefix, &verified);
    });

    if (!subscription) {
      NSString* description = [NSString
          stringWithFormat:@"It wasn't possible to add the verification "
                           @"javascript for element %@",
                           selector.selectorDescription];
      NSDictionary* user_info = @{NSLocalizedDescriptionKey : description};
      *error = [NSError errorWithDomain:kGREYInteractionErrorDomain
                                   code:kGREYInteractionActionFailedErrorCode
                               userInfo:user_info];
      return NO;
    }

    // Run the action and wait for the UI to settle.
    NSError* actionError = nil;
    [[[GREYElementInteraction alloc]
        initWithElementMatcher:WebViewInWebState(state)]
        performAction:action
                error:&actionError];

    if (actionError) {
      *error = actionError;
      return NO;
    }
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

    // Wait for the verified to trigger and set |verified|.
    NSString* verification_timeout_message =
        [NSString stringWithFormat:@"The action (%@) on element %@ wasn't "
                                   @"verified before timing out.",
                                   action.name, selector.selectorDescription];
    bool success = base::test::ios::WaitUntilConditionOrTimeout(
        kWaitForVerificationTimeout, ^{
          return verified;
        });

    if (!success || !verified) {
      DLOG(WARNING) << base::SysNSStringToUTF8(verification_timeout_message);
      return NO;
    }

    return YES;
  };

  return [GREYActionBlock actionWithName:action_name
                             constraints:WebViewInWebState(state)
                            performBlock:verified_tap];
}

id<GREYAction> WebViewLongPressElementForContextMenu(
    WebState* state,
    ElementSelector* selector,
    bool triggers_context_menu) {
  CGRect rect = web::test::GetBoundingRectOfElement(state, selector);
  if (CGRectIsEmpty(rect)) {
    return WebViewElementNotFound(selector);
  }
  CGPoint point = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
  id<GREYAction> longpress =
      grey_longPressAtPointWithDuration(point, kContextMenuLongPressDuration);
  if (triggers_context_menu) {
    return longpress;
  }
  return WebViewVerifiedActionOnElement(state, longpress, selector);
}

id<GREYAction> WebViewTapElement(WebState* state, ElementSelector* selector) {
  CGRect rect = web::test::GetBoundingRectOfElement(state, selector);
  CGPoint point = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
  return CGRectIsEmpty(rect) ? WebViewElementNotFound(selector)
                             : WebViewVerifiedActionOnElement(
                                   state, grey_tapAtPoint(point), selector);
}

id<GREYAction> WebViewScrollElementToVisible(WebState* state,
                                             ElementSelector* selector) {
  const char kScrollToVisibleTemplate[] = "%1$s.scrollIntoView();";

  std::string selector_script =
      base::SysNSStringToUTF8(selector.selectorScript);
  const std::string kScrollToVisibleScript =
      base::StringPrintf(kScrollToVisibleTemplate, selector_script.c_str());

  NSString* action_name =
      [NSString stringWithFormat:@"Scroll element %@ to visible",
                                 selector.selectorDescription];

  NSError* (^error_block)(NSString* error) = ^NSError*(NSString* error) {
    return [NSError errorWithDomain:kGREYInteractionErrorDomain
                               code:kGREYInteractionActionFailedErrorCode
                           userInfo:@{NSLocalizedDescriptionKey : error}];
  };

  GREYActionBlock* scroll_to_visible = [GREYActionBlock
      actionWithName:action_name
         constraints:WebViewInWebState(state)
        performBlock:^BOOL(id element, __strong NSError** error_or_nil) {
          // Checks that the element is indeed a WKWebView.
          WKWebView* web_view = base::mac::ObjCCast<WKWebView>(element);
          if (!web_view) {
            *error_or_nil = error_block(@"WebView not found.");
            return NO;
          }

          __block BOOL success = NO;
          // GREYPerformBlock executes on background thread by default in EG2.
          // Dispatch any call involving UI API to UI thread as they can't be
          // executed on background thread. See
          // go/eg2-migration#greyactions-threading-behavior
          grey_dispatch_sync_on_main_thread(^{
            // First checks if there is really a need to scroll, if the element
            // is already visible just returns early.
            CGRect rect = web::test::GetBoundingRectOfElement(state, selector);
            if (CGRectIsEmpty(rect)) {
              *error_or_nil = error_block(@"Element not found.");
              return;
            }
            if (IsRectVisibleInView(rect, web_view)) {
              success = YES;
              return;
            }

            // Ask the element to scroll itself into view.
            web::test::ExecuteJavaScript(state, kScrollToVisibleScript);

            // Wait until the element is visible.
            bool check = base::test::ios::WaitUntilConditionOrTimeout(
                base::test::ios::kWaitForUIElementTimeout, ^{
                  CGRect rect =
                      web::test::GetBoundingRectOfElement(state, selector);
                  return IsRectVisibleInView(rect, web_view);
                });

            if (!check) {
              *error_or_nil = error_block(@"Element still not visible.");
              return;
            }
            success = YES;
          });

          return success;
        }];

  return scroll_to_visible;
}

}  // namespace web
