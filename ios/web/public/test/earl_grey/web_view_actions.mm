// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/web_view_actions.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

using web::test::ExecuteJavaScript;

namespace {

// Long press duration to trigger context menu.
constexpr base::TimeDelta kContextMenuLongPressDuration = base::Seconds(1);

// Duration to wait for verification of JavaScript action.
// TODO(crbug.com/41289402): Reduce duration if the time required for
// verification is reduced on devices.
constexpr base::TimeDelta kWaitForVerificationTimeout = base::Seconds(8);

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

  GREYPerformBlock verified_tap = ^BOOL(id element, __strong NSError** error) {
    NSString* verifier_script = [NSString
        stringWithFormat:
            @"return await new Promise((resolve) => {"
             "  var element = %@;"
             "  if (!element) {"
             "    resolve('No element');"
             "  }"
             "  const timeoutId = setTimeout(() => {"
             "    resolve(false);"
             // JS timeout slightly shorter than `kWaitForVerificationTimeout`
             "   }, 7900);"
             "  var options = { 'capture': true, 'once': true, 'passive': true "
             "};"
             "  element.addEventListener('mousedown', function(event) {"
             "    clearTimeout(timeoutId);"
             "    resolve(true);"
             "  }, options);"
             "});",
            selector.selectorScript];

    __block bool async_call_complete = false;
    __block bool verified = false;
    // GREYPerformBlock executes on background thread by default in EG2.
    // Dispatch any call involving UI API to UI thread as they can't be executed
    // on background thread. See go/eg2-migration#greyactions-threading-behavior
    grey_dispatch_sync_on_main_thread(^{
      WKWebView* web_view =
          [web::test::GetWebController(state) ensureWebViewCreated];

      [web_view
          callAsyncJavaScript:verifier_script
                    arguments:nil
                      inFrame:nil
               inContentWorld:[WKContentWorld pageWorld]
            completionHandler:^(id result, NSError* async_error) {
              if (!async_error) {
                if ([result isKindOfClass:[NSString class]]) {
                  DLOG(ERROR) << base::SysNSStringToUTF8(result);
                } else if ([result isKindOfClass:[NSNumber class]]) {
                  verified = [result boolValue];
                }
              }
              async_call_complete = true;
            }];
    });

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

    // Wait for the verified to trigger and set `verified`.
    bool success = base::test::ios::WaitUntilConditionOrTimeout(
        kWaitForVerificationTimeout, ^{
          return async_call_complete;
        });

    if (!success || !verified) {
      DLOG(WARNING) << base::SysNSStringToUTF8([NSString
          stringWithFormat:@"The action (%@) on element %@ wasn't "
                           @"verified before timing out.",
                           action.name, selector.selectorDescription]);
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
  id<GREYAction> longpress = grey_longPressAtPointWithDuration(
      point, kContextMenuLongPressDuration.InSecondsF());
  if (triggers_context_menu) {
    return longpress;
  }
  return WebViewVerifiedActionOnElement(state, longpress, selector);
}

id<GREYAction> WebViewTapElement(WebState* state,
                                 ElementSelector* selector,
                                 bool verified) {
  CGRect rect = web::test::GetBoundingRectOfElement(state, selector);
  if (CGRectIsEmpty(rect)) {
    return WebViewElementNotFound(selector);
  }
  CGPoint point = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));

  id<GREYAction> tap_action = grey_tapAtPoint(point);
  if (!verified) {
    return tap_action;
  }
  return WebViewVerifiedActionOnElement(state, tap_action, selector);
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
          WKWebView* web_view = base::apple::ObjCCast<WKWebView>(element);
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
                  CGRect newRect =
                      web::test::GetBoundingRectOfElement(state, selector);
                  return IsRectVisibleInView(newRect, web_view);
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
