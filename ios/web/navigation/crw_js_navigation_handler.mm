// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_js_navigation_handler.h"

#import "base/json/string_escape.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/history_state_util.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/apple/url_conversions.h"

namespace {

// URLs that are fed into WKWebView as history push/replace get escaped,
// potentially changing their format. Code that attempts to determine whether a
// URL hasn't changed can be confused by those differences though, so method
// will round-trip a URL through the escaping process so that it can be adjusted
// pre-storing, to allow later comparisons to work as expected.
GURL URLEscapedForHistory(const GURL& url) {
  // TODO(crbug.com/41464782): This is a very large hammer; see if limited
  // unicode escaping would be sufficient.
  return net::GURLWithNSURL(net::NSURLWithGURL(url));
}

}  // namespace

@implementation CRWJSNavigationHandler

#pragma mark - Public

- (void)handleNavigationWillChangeState {
  self.changingHistoryState = YES;
}

- (void)handleNavigationDidPushStateMessage:(base::Value::Dict*)dict
                                   webState:(web::WebStateImpl*)webStateImpl
                             hasUserGesture:(BOOL)hasUserGesture
                       userInteractionState:
                           (web::UserInteractionState*)userInteractionState
                                 currentURL:(GURL)currentURL {
  if (!webStateImpl || webStateImpl->IsBeingDestroyed()) {
    // Ignore messages received after WebState is being destroyed.
    return;
  }

  DCHECK(self.changingHistoryState);
  self.changingHistoryState = NO;

  const web::NavigationManagerImpl& navigationManagerImpl =
      webStateImpl->GetNavigationManagerImpl();

  // If there is a pending entry, a new navigation has been registered but
  // hasn't begun loading.  Since the pushState message is coming from the
  // previous page, ignore it and allow the previously registered navigation to
  // continue.  This can ocur if a pushState is issued from an anchor tag
  // onClick event, as the click would have already been registered.
  if (navigationManagerImpl.GetPendingItem()) {
    return;
  }

  const std::string* pageURL = dict->FindString("pageUrl");
  const std::string* baseURL = dict->FindString("baseUrl");
  if (!pageURL || !baseURL) {
    DLOG(WARNING) << "JS message parameter not found: pageUrl or baseUrl";
    return;
  }
  GURL pushURL = web::history_state_util::GetHistoryStateChangeUrl(
      currentURL, GURL(*baseURL), *pageURL);
  // UIWebView seems to choke on unicode characters that haven't been
  // escaped; escape the URL now so the expected load URL is correct.
  pushURL = URLEscapedForHistory(pushURL);
  if (!pushURL.is_valid())
    return;

  web::NavigationItemImpl* navItem = navigationManagerImpl.GetCurrentItemImpl();
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
  const std::string* stateObjectJSON = dict->FindString("stateObject");
  if (!stateObjectJSON) {
    DLOG(WARNING) << "JS message parameter not found: stateObject";
    return;
  }
  NSString* stateObject = base::SysUTF8ToNSString(*stateObjectJSON);

  int currentIndex = navigationManagerImpl.GetIndexOfItem(navItem);
  if (currentIndex > 0) {
    web::NavigationItem* previousItem =
        navigationManagerImpl.GetItemAtIndex(currentIndex - 1);
    web::UserAgentType userAgent = previousItem->GetUserAgentType();
    if (userAgent != web::UserAgentType::NONE) {
      navItem->SetUserAgentType(userAgent);
    }
  }
  // If the user interacted with the page, categorize it as a link navigation.
  // If not, categorize it is a client redirect as it occurred without user
  // input and should not be added to the history stack.
  // TODO(crbug.com/41213462): Improve transition detection.
  ui::PageTransition transition =
      userInteractionState->UserInteractionRegisteredSincePageLoaded()
          ? ui::PAGE_TRANSITION_LINK
          : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  [self pushStateWithPageURL:pushURL
                 stateObject:stateObject
                  transition:transition
              hasUserGesture:hasUserGesture
        userInteractionState:userInteractionState
                    webState:webStateImpl];
}

- (void)handleNavigationDidReplaceStateMessage:(base::Value::Dict*)dict
                                      webState:(web::WebStateImpl*)webStateImpl
                                hasUserGesture:(BOOL)hasUserGesture
                          userInteractionState:
                              (web::UserInteractionState*)userInteractionState
                                    currentURL:(GURL)currentURL {
  if (!webStateImpl || webStateImpl->IsBeingDestroyed()) {
    // Ignore messages received after WebState is being destroyed.
    return;
  }

  DCHECK(self.changingHistoryState);
  self.changingHistoryState = NO;

  const std::string* pageURL = dict->FindString("pageUrl");
  const std::string* baseURL = dict->FindString("baseUrl");
  if (!pageURL || !baseURL) {
    DLOG(WARNING) << "JS message parameter not found: pageUrl or baseUrl";
    return;
  }
  GURL replaceURL = web::history_state_util::GetHistoryStateChangeUrl(
      currentURL, GURL(*baseURL), *pageURL);
  // UIWebView seems to choke on unicode characters that haven't been
  // escaped; escape the URL now so the expected load URL is correct.
  replaceURL = URLEscapedForHistory(replaceURL);
  if (!replaceURL.is_valid())
    return;

  const web::NavigationManagerImpl& navigationManagerImpl =
      webStateImpl->GetNavigationManagerImpl();
  web::NavigationItemImpl* navItem = navigationManagerImpl.GetCurrentItemImpl();
  // ReplaceState happened before first navigation entry or called right
  // after window.open when the url is empty/not valid.
  if (!navItem || (navigationManagerImpl.GetItemCount() <= 1 &&
                   navItem->GetURL().is_empty()))
    return;
  if (!web::history_state_util::IsHistoryStateChangeValid(navItem->GetURL(),
                                                          replaceURL)) {
    // If the current session entry URL origin still doesn't match
    // replaceURL's origin, ignore the replaceState. This can happen if a
    // new URL is loaded just before the replaceState.
    return;
  }
  const std::string* stateObjectJSON = dict->FindString("stateObject");
  if (!stateObjectJSON) {
    DLOG(WARNING) << "JS message parameter not found: stateObject";
    return;
  }
  NSString* stateObject = base::SysUTF8ToNSString(*stateObjectJSON);
  [self replaceStateWithPageURL:replaceURL
                    stateObject:stateObject
                 hasUserGesture:hasUserGesture
                       webState:webStateImpl];
}

#pragma mark - Private

// Adds a new NavigationItem with the given URL and state object to the
// history stack. A state object is a serialized generic JavaScript object
// that contains details of the UI's state for a given NavigationItem/URL.
// TODO(crbug.com/40624624): Move the pushState/replaceState logic into
// NavigationManager.
- (void)pushStateWithPageURL:(const GURL&)pageURL
                 stateObject:(NSString*)stateObject
                  transition:(ui::PageTransition)transition
              hasUserGesture:(BOOL)hasUserGesture
        userInteractionState:(web::UserInteractionState*)userInteractionState
                    webState:(web::WebStateImpl*)webStateImpl {
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          webStateImpl, pageURL, hasUserGesture, transition,
          /*is_renderer_initiated=*/true);
  context->SetIsSameDocument(true);
  webStateImpl->OnNavigationStarted(context.get());
  context->SetHasCommitted(true);
  webStateImpl->OnNavigationFinished(context.get());
  userInteractionState->SetUserInteractionRegisteredSincePageLoaded(false);
}

// Assigns the given URL and state object to the current NavigationItem.
- (void)replaceStateWithPageURL:(const GURL&)pageURL
                    stateObject:(NSString*)stateObject
                 hasUserGesture:(BOOL)hasUserGesture
                       webState:(web::WebStateImpl*)webStateImpl {
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          webStateImpl, pageURL, hasUserGesture,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*is_renderer_initiated=*/true);
  context->SetIsSameDocument(true);
  webStateImpl->OnNavigationStarted(context.get());
  web::NavigationManagerImpl& navigationManagerImpl =
      webStateImpl->GetNavigationManagerImpl();
  navigationManagerImpl.UpdateCurrentItemForReplaceState(pageURL, stateObject);
  context->SetHasCommitted(true);
  webStateImpl->OnNavigationFinished(context.get());
}

@end
