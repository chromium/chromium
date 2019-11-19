// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_context_menu_controller.h"

#import <objc/runtime.h>
#include <stddef.h>

#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/unguessable_token.h"
#import "ios/web/js_messaging/crw_wk_script_message_router.h"
#import "ios/web/public/deprecated/crw_context_menu_delegate.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web/web_state/context_menu_constants.h"
#import "ios/web/web_state/context_menu_params_utils.h"
#import "ios/web/web_state/ui/html_element_fetch_request.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The long press detection duration must be shorter than the WKWebView's
// long click gesture recognizer's minimum duration. That is 0.55s.
// If our detection duration is shorter, our gesture recognizer will fire
// first in order to cancel the system context menu gesture recognizer.
const NSTimeInterval kLongPressDurationSeconds = 0.55 - 0.1;
// Since iOS 13, our gesture recognizer needs to allow enough time for drag and
// drop to trigger first.
const NSTimeInterval kLongPressDurationSecondsIOS13 = 0.75;

// If there is a movement bigger than |kLongPressMoveDeltaPixels|, the context
// menu will not be triggered.
const CGFloat kLongPressMoveDeltaPixels = 10.0;

// Cancels touch events for the given gesture recognizer.
void CancelTouches(UIGestureRecognizer* gesture_recognizer) {
  if (gesture_recognizer.enabled) {
    gesture_recognizer.enabled = NO;
    gesture_recognizer.enabled = YES;
  }
}

// JavaScript message handler name installed in WKWebView for found element
// response.
NSString* const kFindElementResultHandlerName = @"FindElementResultHandler";

// Enum used to record element details fetched for the context menu.
enum class ContextMenuElementFrame {
  // Recorded when the element was found in the main frame.
  MainFrame = 0,
  // Recorded when the element was found in an iframe.
  Iframe = 1,
  Count
};

// Name of the histogram for recording when the gesture recognizer recognizes a
// long press before the DOM element details are available.
const char kContextMenuDelayedElementDetailsHistogram[] =
    "ContextMenu.DelayedElementDetails";

// Enum used to record resulting action when the gesture recognizer recognizes a
// long press before the DOM element details are available.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DelayedElementDetailsState {
  // Recorded when the context menu is displayed when receiving the dom element
  // details after the gesture recognizer had already recognized a long press.
  Show = 0,
  // Recorded when the context menu is not displayed after the gesture
  // recognizer fully recognized a long press.
  Cancel = 1,
  kMaxValue = Cancel
};

// Struct to track the details of the element at |location| in |webView|.
struct ContextMenuInfo {
  // The location of the long press.
  CGPoint location;
  // True if the element is in the page's main frame, false if in an iframe.
  BOOL is_main_frame;
  // DOM element information. May contain the keys defined in
  // ios/web/web_state/context_menu_constants.h. All values are strings.
  NSDictionary* dom_element;
};

// Returns an array of gesture recognizers with |fragment| in it's description
// and attached to a subview of |webView|.
NSArray<UIGestureRecognizer*>* GestureRecognizersWithDescriptionFragment(
    NSString* fragment,
    WKWebView* webView) {
  NSMutableArray* matches = [[NSMutableArray alloc] init];
  for (UIView* view in [[webView scrollView] subviews]) {
    for (UIGestureRecognizer* recognizer in [view gestureRecognizers]) {
      NSString* recognizerDescription = recognizer.description;
      NSRange mustFailRange =
          [recognizerDescription rangeOfString:@"must-fail"];
      if (mustFailRange.length) {
        // Strip off description of other recognizers to ensure |fragment| only
        // matches properties of |recognizer|.
        recognizerDescription =
            [recognizerDescription substringToIndex:mustFailRange.location];
      }
      if ([recognizerDescription rangeOfString:fragment
                                       options:NSCaseInsensitiveSearch]
              .length) {
        [matches addObject:recognizer];
      }
    }
  }
  return matches;
}

// WKWebView's default gesture recognizers interfere with the detection of a
// long press by |_contextMenuRecognizer|. Calling this method ensures that
// WKWebView's gesture recognizers for context menu and text selection should
// fail if |_contextMenuRecognizer| detects a long press.
void OverrideGestureRecognizers(UIGestureRecognizer* contextMenuRecognizer,
                                WKWebView* webView) {
  NSMutableArray<UIGestureRecognizer*>* recognizers =
      [[NSMutableArray alloc] init];
  if (@available(iOS 13, *)) {
    [recognizers
        addObjectsFromArray:GestureRecognizersWithDescriptionFragment(
                                @"com.apple.UIKit.clickPresentationFailure",
                                webView)];
    [recognizers addObjectsFromArray:GestureRecognizersWithDescriptionFragment(
                                         @"Text", webView)];

  } else {
    [recognizers
        addObjectsFromArray:GestureRecognizersWithDescriptionFragment(
                                @"action=_longPressRecognized:", webView)];
  }

  for (UIGestureRecognizer* systemRecognizer in recognizers) {
    [systemRecognizer requireGestureRecognizerToFail:contextMenuRecognizer];
    // requireGestureRecognizerToFail: doesn't retain the recognizer, so it is
    // possible for |systemContextMenuRecognizer| to outlive
    // |contextMenuRecognizer| and end up with a dangling pointer. Add a
    // retaining associative reference to ensure that the lifetimes work out.
    // Note that normally using the value as the key wouldn't make any sense,
    // but here it's fine since nothing needs to look up the value.
    void* associatedObjectKey = (__bridge void*)systemRecognizer;
    objc_setAssociatedObject(systemRecognizer.view, associatedObjectKey,
                             contextMenuRecognizer,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);

    if (!base::ios::IsRunningOnIOS13OrLater()) {
      // Retain behavior implemented for iOS 12 by only requiring the first
      // matching gesture recognizer to fail.
      break;
    }
  }
}

}  // namespace

@interface CRWContextMenuController () <CRWWebStateObserver,
                                        UIGestureRecognizerDelegate>

// The |webView|.
@property(nonatomic, readonly, weak) WKWebView* webView;
// The scroll view of |webView|.
@property(nonatomic, readonly, weak) id<CRWContextMenuDelegate> delegate;
// Returns the x, y offset the content has been scrolled.
@property(nonatomic, readonly) CGPoint scrollPosition;

// Called when the |_contextMenuRecognizer| finishes recognizing a long press.
- (void)longPressDetectedByGestureRecognizer:
    (UIGestureRecognizer*)gestureRecognizer;
// Called when the |_contextMenuRecognizer| begins recognizing a long press.
- (void)longPressGestureRecognizerBegan;
// Called when the |_contextMenuRecognizer| changes.
- (void)longPressGestureRecognizerChanged;
// Show the context menu or allow the system default behavior based on the DOM
// element details in |_contextMenuInfoForLastTouch.dom_element|.
- (void)processReceivedDOMElement;
// Called when the context menu must be shown.
- (void)showContextMenu;
// Cancels all touch events in the web view (long presses, tapping, scrolling).
- (void)cancelAllTouches;
// Asynchronously fetches information about DOM element for the given point (in
// UIView coordinates). |handler| can not be nil. See
// |_contextMenuInfoForLastTouch.dom_element| for element format description.
- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:(void (^)(NSDictionary*))handler;
// Sets the value of |_contextMenuInfoForLastTouch.dom_element|.
- (void)setDOMElementForLastTouch:(NSDictionary*)element;
// Called to process a message received from JavaScript.
- (void)didReceiveScriptMessage:(WKScriptMessage*)message;
// Cancels the display of the context menu and clears associated element fetch
// request state.
- (void)cancelContextMenuDisplay;
// Forwards the execution of |script| to |javaScriptDelegate| and if it is nil,
// to |webView|.
- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler;
@end

@implementation CRWContextMenuController {
  std::unique_ptr<web::WebStateObserverBridge> _observer;
  // Long press recognizer that allows showing context menus.
  UILongPressGestureRecognizer* _contextMenuRecognizer;
  // DOM element information for the point where the user made the last touch.
  // Precalculation is necessary because retreiving DOM element relies on async
  // API so element info can not be built on demand.
  ContextMenuInfo _contextMenuInfoForLastTouch;
  // Whether or not the cotext menu should be displayed as soon as the DOM
  // element details are returned. Since fetching the details from the |webView|
  // of the element the user long pressed is asyncrounous, it may not be
  // complete by the time the context menu gesture recognizer is complete.
  // |_contextMenuNeedsDisplay| is set to YES to indicate the
  // |_contextMenuRecognizer| finished, but couldn't yet show the context menu
  // becuase the DOM element details were not yet available.
  BOOL _contextMenuNeedsDisplay;
  // Details for currently in progress element fetches. The objects are
  // instances of HTMLElementFetchRequest and are keyed by a unique requestId
  // string.
  NSMutableDictionary* _pendingElementFetchRequests;
}

@synthesize delegate = _delegate;
@synthesize webView = _webView;

- (instancetype)initWithWebView:(WKWebView*)webView
                   browserState:(web::BrowserState*)browserState
                       delegate:(id<CRWContextMenuDelegate>)delegate {
  DCHECK(webView);
  self = [super init];
  if (self) {
    _webView = webView;
    _delegate = delegate;
    _pendingElementFetchRequests = [[NSMutableDictionary alloc] init];

    // The system context menu triggers after 0.55 second. Add a gesture
    // recognizer with a shorter delay to be able to cancel the system menu if
    // needed.
    _contextMenuRecognizer = [[UILongPressGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(longPressDetectedByGestureRecognizer:)];

    if (@available(iOS 13, *)) {
      [_contextMenuRecognizer
          setMinimumPressDuration:kLongPressDurationSecondsIOS13];
    } else {
      [_contextMenuRecognizer
          setMinimumPressDuration:kLongPressDurationSeconds];
    }
    [_contextMenuRecognizer setAllowableMovement:kLongPressMoveDeltaPixels];
    [_contextMenuRecognizer setDelegate:self];
    [_webView addGestureRecognizer:_contextMenuRecognizer];

    OverrideGestureRecognizers(_contextMenuRecognizer, _webView);

    // Listen for fetched element response.
    web::WKWebViewConfigurationProvider& configurationProvider =
        web::WKWebViewConfigurationProvider::FromBrowserState(browserState);
    CRWWKScriptMessageRouter* messageRouter =
        configurationProvider.GetScriptMessageRouter();
    __weak CRWContextMenuController* weakSelf = self;
    [messageRouter setScriptMessageHandler:^(WKScriptMessage* message) {
      [weakSelf didReceiveScriptMessage:message];
    }
                                      name:kFindElementResultHandlerName
                                   webView:webView];
  }
  return self;
}

- (void)dealloc {
  if (_webState)
    _webState->RemoveObserver(_observer.get());
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState)
    _webState->RemoveObserver(_observer.get());

  _webState = webState;
  _observer.reset();

  if (webState) {
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    webState->AddObserver(_observer.get());
  }
}

- (void)allowSystemUIForCurrentGesture {
  // Reset the state of the recognizer so that it doesn't recognize the on-going
  // touch.
  _contextMenuRecognizer.enabled = NO;
  _contextMenuRecognizer.enabled = YES;
}

- (UIScrollView*)webScrollView {
  return [_webView scrollView];
}

- (CGPoint)scrollPosition {
  return self.webScrollView.contentOffset;
}

- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler {
  if ([_delegate respondsToSelector:@selector
                 (webView:executeJavaScript:completionHandler:)]) {
    [_delegate webView:self.webView
        executeJavaScript:script
        completionHandler:completionHandler];
  } else {
    [self.webView evaluateJavaScript:script
                   completionHandler:completionHandler];
  }
}

- (void)longPressDetectedByGestureRecognizer:
    (UIGestureRecognizer*)gestureRecognizer {
  switch (gestureRecognizer.state) {
    case UIGestureRecognizerStateBegan:
      [self longPressGestureRecognizerBegan];
      break;
    case UIGestureRecognizerStateEnded:
      [self cancelContextMenuDisplay];
      break;
    case UIGestureRecognizerStateChanged:
      [self longPressGestureRecognizerChanged];
      break;
    default:
      break;
  }
}

- (void)longPressGestureRecognizerBegan {
  if (_contextMenuInfoForLastTouch.dom_element) {
    [self processReceivedDOMElement];
  } else {
    // Shows the context menu once the DOM element information is set.
    _contextMenuNeedsDisplay = YES;
    UMA_HISTOGRAM_BOOLEAN("ContextMenu.WaitingForElementDetails", true);
  }
}

- (void)longPressGestureRecognizerChanged {
  if (!_contextMenuNeedsDisplay ||
      CGPointEqualToPoint(_contextMenuInfoForLastTouch.location, CGPointZero)) {
    return;
  }

  // If the user moved more than kLongPressMoveDeltaPixels along either asis
  // after the gesture was already recognized, the context menu should not be
  // shown. The distance variation needs to be manually cecked if
  // |_contextMenuNeedsDisplay| has already been set to True.
  CGPoint currentTouchLocation =
      [_contextMenuRecognizer locationInView:_webView];
  float deltaX = std::abs(_contextMenuInfoForLastTouch.location.x -
                          currentTouchLocation.x);
  float deltaY = std::abs(_contextMenuInfoForLastTouch.location.y -
                          currentTouchLocation.y);
  if (deltaX > kLongPressMoveDeltaPixels ||
      deltaY > kLongPressMoveDeltaPixels) {
    [self cancelContextMenuDisplay];
  }
}

- (void)processReceivedDOMElement {
  BOOL canShowContextMenu = web::CanShowContextMenuForElementDictionary(
      _contextMenuInfoForLastTouch.dom_element);
  if (!canShowContextMenu) {
    // There is no link or image under user's gesture. Do not cancel all touches
    // to allow system text selection UI.
    [self allowSystemUIForCurrentGesture];
    return;
  }

  // User long pressed on a link or an image. Cancelling all touches will
  // intentionally suppress system context menu UI.
  [self cancelAllTouches];

  if ([_delegate respondsToSelector:@selector(webView:handleContextMenu:)]) {
    _contextMenuInfoForLastTouch.location =
        [_contextMenuRecognizer locationInView:_webView];
    [self showContextMenu];
  }
}

- (void)showContextMenu {
  // Log if the element is in the main frame or a child frame.
  UMA_HISTOGRAM_ENUMERATION("ContextMenu.DOMElementFrame",
                            (_contextMenuInfoForLastTouch.is_main_frame
                                 ? ContextMenuElementFrame::MainFrame
                                 : ContextMenuElementFrame::Iframe),
                            ContextMenuElementFrame::Count);

  web::ContextMenuParams params = web::ContextMenuParamsFromElementDictionary(
      _contextMenuInfoForLastTouch.dom_element);
  params.view = _webView;
  params.location = _contextMenuInfoForLastTouch.location;
  [_delegate webView:_webView handleContextMenu:params];
}

- (void)cancelAllTouches {
  UMA_HISTOGRAM_BOOLEAN("ContextMenu.CancelSystemTouches", true);

  // Disable web view scrolling.
  CancelTouches(self.webView.scrollView.panGestureRecognizer);

  // All user gestures are handled by a subview of web view scroll view
  // (WKContentView).
  for (UIView* subview in self.webScrollView.subviews) {
    for (UIGestureRecognizer* recognizer in subview.gestureRecognizers) {
      CancelTouches(recognizer);
    }
  }
}

- (void)setDOMElementForLastTouch:(NSDictionary*)element {
  _contextMenuInfoForLastTouch.dom_element = [element copy];
  if (_contextMenuNeedsDisplay) {
    _contextMenuNeedsDisplay = NO;
    UMA_HISTOGRAM_ENUMERATION(kContextMenuDelayedElementDetailsHistogram,
                              DelayedElementDetailsState::Show);
    [self processReceivedDOMElement];
  }
}

- (void)didReceiveScriptMessage:(WKScriptMessage*)message {
  NSMutableDictionary* response =
      [[NSMutableDictionary alloc] initWithDictionary:message.body];
  _contextMenuInfoForLastTouch.is_main_frame = message.frameInfo.mainFrame;
  NSString* requestID = response[web::kContextMenuElementRequestId];
  HTMLElementFetchRequest* fetchRequest =
      _pendingElementFetchRequests[requestID];
  // Do not process the message if a fetch request with a matching |requestID|
  // was not found. This ensures that the response matches a request made by
  // this CRWContextMenuController instance.
  if (fetchRequest) {
    [_pendingElementFetchRequests removeObjectForKey:requestID];
    [fetchRequest runHandlerWithResponse:response];
  }
}

- (void)cancelContextMenuDisplay {
  if (_contextMenuNeedsDisplay) {
    UMA_HISTOGRAM_ENUMERATION(kContextMenuDelayedElementDetailsHistogram,
                              DelayedElementDetailsState::Cancel);
  }
  _contextMenuNeedsDisplay = NO;
  _contextMenuInfoForLastTouch.location = CGPointZero;
  for (HTMLElementFetchRequest* fetchRequest in _pendingElementFetchRequests
           .allValues) {
    [fetchRequest invalidate];
  }
}

#pragma mark -
#pragma mark UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Allows the custom UILongPressGestureRecognizer to fire simultaneously with
  // other recognizers.
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Expect only _contextMenuRecognizer.
  DCHECK([gestureRecognizer isEqual:_contextMenuRecognizer]);

  // This is custom long press gesture recognizer. By the time the gesture is
  // recognized the web controller needs to know if there is a link under the
  // touch. If there a link, the web controller will reject system's context
  // menu and show another one. If for some reason context menu info is not
  // fetched - system context menu will be shown.
  [self setDOMElementForLastTouch:nil];
  [self cancelContextMenuDisplay];

  __weak CRWContextMenuController* weakSelf = self;
  [self fetchDOMElementAtPoint:[touch locationInView:_webView]
             completionHandler:^(NSDictionary* element) {
               [weakSelf setDOMElementForLastTouch:element];
             }];
  return YES;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  // Expect only _contextMenuRecognizer.
  DCHECK([gestureRecognizer isEqual:_contextMenuRecognizer]);

  // Context menu should not be triggered while scrolling, as some users tend to
  // stop scrolling by resting the finger on the screen instead of touching the
  // screen. For more info, please refer to crbug.com/642375.
  if ([self webScrollView].dragging) {
    return NO;
  }

  return YES;
}

#pragma mark -
#pragma mark Web Page Features

- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:(void (^)(NSDictionary*))handler {
  DCHECK(handler);

  CGPoint scrollOffset = self.scrollPosition;
  CGSize webViewContentSize = self.webScrollView.contentSize;
  CGFloat webViewContentWidth = webViewContentSize.width;
  CGFloat webViewContentHeight = webViewContentSize.height;

  NSString* requestID =
      base::SysUTF8ToNSString(base::UnguessableToken::Create().ToString());
  HTMLElementFetchRequest* fetchRequest =
      [[HTMLElementFetchRequest alloc] initWithFoundElementHandler:handler];
  _pendingElementFetchRequests[requestID] = fetchRequest;
  NSString* formatString = [NSString
      stringWithFormat:
          @"__gCrWeb.findElementAtPoint('%@', %%g, %%g, %%g, %%g);", requestID];

  NSString* getElementScript =
      [NSString stringWithFormat:formatString, point.x + scrollOffset.x,
                                 point.y + scrollOffset.y, webViewContentWidth,
                                 webViewContentHeight];
  [self executeJavaScript:getElementScript completionHandler:nil];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  self.webState = nullptr;
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  if (@available(iOS 13, *)) {
    if (!navigation->IsSameDocument() && navigation->HasCommitted())
      OverrideGestureRecognizers(_contextMenuRecognizer, _webView);
  }
}

@end
