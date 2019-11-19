// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_js_navigation_handler.h"

#include "base/json/string_escape.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/history_state_util.h"
#import "ios/web/js_messaging/crw_js_injector.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kCommandPrefix[] = "navigation";

// URLs that are fed into WKWebView as history push/replace get escaped,
// potentially changing their format. Code that attempts to determine whether a
// URL hasn't changed can be confused by those differences though, so method
// will round-trip a URL through the escaping process so that it can be adjusted
// pre-storing, to allow later comparisons to work as expected.
GURL URLEscapedForHistory(const GURL& url) {
  // TODO(crbug.com/973871): This is a very large hammer; see if limited unicode
  // escaping would be sufficient.
  return net::GURLWithNSURL(net::NSURLWithGURL(url));
}

}  // namespace

@interface CRWJSNavigationHandler () {
  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> _subscription;
}

@property(nonatomic, weak) id<CRWJSNavigationHandlerDelegate> delegate;

// Sets to YES when |close| is called.
@property(nonatomic, assign) BOOL beingDestroyed;

// Returns WebStateImpl from self.delegate.
@property(nonatomic, readonly, assign) web::WebStateImpl* webStateImpl;
// Returns NavigationManagerImpl from self.webStateImpl.
@property(nonatomic, readonly, assign)
    web::NavigationManagerImpl* navigationManagerImpl;
// Returns UserInteractionState from self.delegate.
@property(nonatomic, readonly, assign)
    web::UserInteractionState* userInteractionState;
// Returns WKWebView from self.delegate.
@property(nonatomic, readonly, weak) WKWebView* webView;
// Returns current URL from self.delegate.
@property(nonatomic, readonly, assign) GURL currentURL;

@end

@implementation CRWJSNavigationHandler

#pragma mark - Public

- (instancetype)initWithDelegate:(id<CRWJSNavigationHandlerDelegate>)delegate {
  if (self = [super init]) {
    _delegate = delegate;

    __weak CRWJSNavigationHandler* weakSelf = self;
    auto navigationStateCallback = ^(
        const base::DictionaryValue& message, const GURL&,
        bool /* user_is_interacting */, web::WebFrame* senderFrame) {
      if (!senderFrame->IsMainFrame())
        return;

      const std::string* command = message.FindStringKey("command");
      DCHECK(command);
      if (*command == "navigation.hashchange") {
        [weakSelf handleNavigationHashChangeInFrame:senderFrame];
      } else if (*command == "navigation.willChangeState") {
        [weakSelf handleNavigationWillChangeStateInFrame:senderFrame];
      } else if (*command == "navigation.didPushState") {
        [weakSelf handleNavigationDidPushStateMessage:message
                                              inFrame:senderFrame];
      } else if (*command == "navigation.didReplaceState") {
        [weakSelf handleNavigationDidReplaceStateMessage:message
                                                 inFrame:senderFrame];
      } else if (*command == "navigation.goDelta") {
        [weakSelf handleNavigationGoDeltaMessage:message inFrame:senderFrame];
      }
    };

    _subscription = self.webStateImpl->AddScriptCommandCallback(
        base::BindRepeating(navigationStateCallback), kCommandPrefix);
  }
  return self;
}

- (void)close {
  self.beingDestroyed = YES;
}

- (NSString*)javaScriptToReplaceWebViewURL:(const GURL&)URL
                           stateObjectJSON:(NSString*)stateObject {
  std::string outURL;
  base::EscapeJSONString(URL.spec(), true, &outURL);
  return
      [NSString stringWithFormat:@"__gCrWeb.replaceWebViewURL(%@, %@);",
                                 base::SysUTF8ToNSString(outURL), stateObject];
}

#pragma mark - Private

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForJSNavigationHandler:self];
}

- (web::NavigationManagerImpl*)navigationManagerImpl {
  return &(self.webStateImpl->GetNavigationManagerImpl());
}

- (web::UserInteractionState*)userInteractionState {
  return [self.delegate userInteractionStateForJSNavigationHandler:self];
}

- (WKWebView*)webView {
  return [self.delegate webViewForJSNavigationHandler:self];
}

- (GURL)currentURL {
  return [self.delegate currentURLForJSNavigationHandler:self];
}

// Handles the navigation.hashchange event emitted from |senderFrame|.
- (void)handleNavigationHashChangeInFrame:(web::WebFrame*)senderFrame {
  // Record that the current NavigationItem was created by a hash change, but
  // ignore hashchange events that are manually dispatched for same-document
  // navigations.
  if (self.dispatchingSameDocumentHashChangeEvent) {
    self.dispatchingSameDocumentHashChangeEvent = NO;
  } else {
    self.navigationManagerImpl->GetCurrentItemImpl()
        ->SetIsCreatedFromHashChange(true);
  }
}

// Handles the navigation.willChangeState message sent from |senderFrame|.
- (void)handleNavigationWillChangeStateInFrame:(web::WebFrame*)senderFrame {
  self.changingHistoryState = YES;
}

// Handles the navigation.didChangeState message sent from |senderFrame|.
- (void)handleNavigationDidPushStateMessage:
            (const base::DictionaryValue&)message
                                    inFrame:(web::WebFrame*)senderFrame {
  DCHECK(self.changingHistoryState);
  self.changingHistoryState = NO;

  // If there is a pending entry, a new navigation has been registered but
  // hasn't begun loading.  Since the pushState message is coming from the
  // previous page, ignore it and allow the previously registered navigation to
  // continue.  This can ocur if a pushState is issued from an anchor tag
  // onClick event, as the click would have already been registered.
  if (self.navigationManagerImpl->GetPendingItem()) {
    return;
  }

  const std::string* pageURL = message.FindStringKey("pageUrl");
  const std::string* baseURL = message.FindStringKey("baseUrl");
  if (!pageURL || !baseURL) {
    DLOG(WARNING) << "JS message parameter not found: pageUrl or baseUrl";
    return;
  }
  GURL pushURL = web::history_state_util::GetHistoryStateChangeUrl(
      self.currentURL, GURL(*baseURL), *pageURL);
  // UIWebView seems to choke on unicode characters that haven't been
  // escaped; escape the URL now so the expected load URL is correct.
  pushURL = URLEscapedForHistory(pushURL);
  if (!pushURL.is_valid())
    return;

  web::NavigationItemImpl* navItem =
      self.navigationManagerImpl->GetCurrentItemImpl();
  // PushState happened before first navigation entry or called when the
  // navigation entry does not contain a valid URL.
  if (!navItem || !navItem->GetURL().is_valid())
    return;
  if (!web::history_state_util::IsHistoryStateChangeValid(navItem->GetURL(),
                                                          pushURL)) {
    // If the current session entry URL origin still doesn't match pushURL's
    // origin, ignore the pushState. This can happen if a new URL is loaded
    // just before the pushState.
    return;
  }
  const std::string* stateObjectJSON = message.FindStringKey("stateObject");
  if (!stateObjectJSON) {
    DLOG(WARNING) << "JS message parameter not found: stateObject";
    return;
  }
  NSString* stateObject = base::SysUTF8ToNSString(*stateObjectJSON);

  // If the user interacted with the page, categorize it as a link navigation.
  // If not, categorize it is a client redirect as it occurred without user
  // input and should not be added to the history stack.
  // TODO(crbug.com/549301): Improve transition detection.
  ui::PageTransition transition =
      self.userInteractionState->UserInteractionRegisteredSincePageLoaded()
          ? ui::PAGE_TRANSITION_LINK
          : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  [self pushStateWithPageURL:pushURL
                 stateObject:stateObject
                  transition:transition
              hasUserGesture:self.userInteractionState->IsUserInteracting(
                                 self.webView)];
  [self.delegate
      JSNavigationHandlerUpdateSSLStatusForCurrentNavigationItem:self];
}

// Handles the navigation.didReplaceState message sent from |senderFrame|.
- (void)handleNavigationDidReplaceStateMessage:
            (const base::DictionaryValue&)message
                                       inFrame:(web::WebFrame*)senderFrame {
  DCHECK(self.changingHistoryState);
  self.changingHistoryState = NO;

  const std::string* pageURL = message.FindStringKey("pageUrl");
  const std::string* baseURL = message.FindStringKey("baseUrl");
  if (!pageURL || !baseURL) {
    DLOG(WARNING) << "JS message parameter not found: pageUrl or baseUrl";
    return;
  }
  GURL replaceURL = web::history_state_util::GetHistoryStateChangeUrl(
      self.currentURL, GURL(*baseURL), *pageURL);
  // UIWebView seems to choke on unicode characters that haven't been
  // escaped; escape the URL now so the expected load URL is correct.
  replaceURL = URLEscapedForHistory(replaceURL);
  if (!replaceURL.is_valid())
    return;

  web::NavigationItemImpl* navItem =
      self.navigationManagerImpl->GetCurrentItemImpl();
  // ReplaceState happened before first navigation entry or called right
  // after window.open when the url is empty/not valid.
  if (!navItem || (self.navigationManagerImpl->GetItemCount() <= 1 &&
                   navItem->GetURL().is_empty()))
    return;
  if (!web::history_state_util::IsHistoryStateChangeValid(navItem->GetURL(),
                                                          replaceURL)) {
    // If the current session entry URL origin still doesn't match
    // replaceURL's origin, ignore the replaceState. This can happen if a
    // new URL is loaded just before the replaceState.
    return;
  }
  const std::string* stateObjectJSON = message.FindStringKey("stateObject");
  if (!stateObjectJSON) {
    DLOG(WARNING) << "JS message parameter not found: stateObject";
    return;
  }
  NSString* stateObject = base::SysUTF8ToNSString(*stateObjectJSON);
  [self replaceStateWithPageURL:replaceURL
                    stateObject:stateObject
                 hasUserGesture:self.userInteractionState->IsUserInteracting(
                                    self.webView)];
  return;
}

// Handles 'navigation.goDelta' message sent from |senderFrame|.
- (void)handleNavigationGoDeltaMessage:(const base::DictionaryValue&)message
                               inFrame:(web::WebFrame*)senderFrame {
  const base::Value* value = message.FindKey("value");
  if (value && value->is_double()) {
    [self rendererInitiatedGoDelta:static_cast<int>(value->GetDouble())
                    hasUserGesture:self.userInteractionState->IsUserInteracting(
                                       self.webView)];
  }
}

// Adds a new NavigationItem with the given URL and state object to the
// history stack. A state object is a serialized generic JavaScript object
// that contains details of the UI's state for a given NavigationItem/URL.
// TODO(crbug.com/956511): Move the pushState/replaceState logic into
// NavigationManager.
- (void)pushStateWithPageURL:(const GURL&)pageURL
                 stateObject:(NSString*)stateObject
                  transition:(ui::PageTransition)transition
              hasUserGesture:(BOOL)hasUserGesture {
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webStateImpl, pageURL, hasUserGesture, transition,
          /*is_renderer_initiated=*/true);
  context->SetIsSameDocument(true);
  self.webStateImpl->OnNavigationStarted(context.get());
  self.navigationManagerImpl->AddPushStateItemIfNecessary(pageURL, stateObject,
                                                          transition);
  context->SetHasCommitted(true);
  self.webStateImpl->OnNavigationFinished(context.get());
  self.userInteractionState->SetUserInteractionRegisteredSincePageLoaded(false);
}

// Assigns the given URL and state object to the current NavigationItem.
- (void)replaceStateWithPageURL:(const GURL&)pageURL
                    stateObject:(NSString*)stateObject
                 hasUserGesture:(BOOL)hasUserGesture {
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webStateImpl, pageURL, hasUserGesture,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*is_renderer_initiated=*/true);
  context->SetIsSameDocument(true);
  self.webStateImpl->OnNavigationStarted(context.get());
  self.navigationManagerImpl->UpdateCurrentItemForReplaceState(pageURL,
                                                               stateObject);
  context->SetHasCommitted(true);
  self.webStateImpl->OnNavigationFinished(context.get());
}

// Navigates forwards or backwards by |delta| pages. No-op if delta is out of
// bounds. Reloads if delta is 0.
// TODO(crbug.com/661316): Move this method to NavigationManager.
- (void)rendererInitiatedGoDelta:(int)delta
                  hasUserGesture:(BOOL)hasUserGesture {
  if (self.beingDestroyed)
    return;

  if (delta == 0) {
    [self.delegate
        JSNavigationHandlerReloadWithRendererInitiatedNavigation:self];
    return;
  }

  if (self.navigationManagerImpl->CanGoToOffset(delta)) {
    int index = self.navigationManagerImpl->GetIndexForOffset(delta);
    self.navigationManagerImpl->GoToIndex(
        index, web::NavigationInitiationType::RENDERER_INITIATED,
        /*has_user_gesture=*/hasUserGesture);
  }
}

@end
