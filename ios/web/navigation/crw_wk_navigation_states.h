// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_STATES_H_
#define IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_STATES_H_

#include <memory>

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

namespace web {

class NavigationContextImpl;

// State of in-flight WKNavigation objects.
enum class WKNavigationState : int {
  // Navigation does not exist.
  NONE = 0,
  // WKNavigation returned from `loadRequest:`, `goToBackForwardListItem:`,
  // `loadFileURL:allowingReadAccessToURL:`, `loadHTMLString:baseURL:`,
  // `loadData:MIMEType:characterEncodingName:baseURL:`, `goBack`, `goForward`,
  // `reload` or `reloadFromOrigin`.
  REQUESTED,
  // WKNavigation passed to `webView:didStartProvisionalNavigation:`.
  STARTED,
  // WKNavigation passed to
  // `webView:didReceiveServerRedirectForProvisionalNavigation:`.
  REDIRECTED,
  // WKNavigation passed to `webView:didFailProvisionalNavigation:`.
  PROVISIONALY_FAILED,
  // WKNavigation passed to `webView:didCommitNavigation:`.
  COMMITTED,
  // WKNavigation passed to `webView:didFinishNavigation:`.
  FINISHED,
  // WKNavigation passed to `webView:didFailNavigation:withError:`.
  FAILED,
};

}  // namespace web

// Stores states and navigation contexts for WKNavigation objects.
// Allows looking up for last added navigation object. null WKNavigation is a
// valid navigation and treated as a unique key. WKWebView passes null
// WKNavigation to WKNavigationDelegate callbacks if navigation represents
// window opening action.
@interface CRWWKNavigationStates : NSObject

// Adds a new navigation if it was not added yet. If navigation was already
// added then updates state for existing navigation. Updating state does not
// affect the result of `lastAddedNavigation` method. New added navigations
// should have WKNavigationState::REQUESTED, WKNavigationState::STARTED or
// WKNavigationState::COMMITTED state. `navigation` will be held as a weak
// reference and will not be retained. No-op if `navigation` is null.
- (void)setState:(web::WKNavigationState)state
    forNavigation:(WKNavigation*)navigation;

// Returns state for a given `navigation` or NONE if navigation does not exist.
- (web::WKNavigationState)stateForNavigation:(WKNavigation*)navigation;

// Removes given `navigation` and returns ownership of the associated navigation
// context. Fails if `navigation` does not exist. `navigation` can be null.
// Cliens don't have to call this method for non-null navigations because
// non-null navigations are weak and will be automatically removed when system
// releases finished navigaitons. This method must always be called for
// completed null navigations because they are not removed automatically.
- (std::unique_ptr<web::NavigationContextImpl>)removeNavigation:
    (WKNavigation*)navigation;

// Adds a new navigation if it was not added yet. If navigation was already
// added then updates context for existing navigation. Updating context does not
// affect the result of `lastAddedNavigation` method. `navigation` will be held
// as a weak reference and will not be retained. No-op if `navigation` is null.
- (void)setContext:(std::unique_ptr<web::NavigationContextImpl>)context
     forNavigation:(WKNavigation*)navigation;

// Returns context if one was previously associated with given `navigation`.
// Returns null if `navigation` is null.
- (web::NavigationContextImpl*)contextForNavigation:(WKNavigation*)navigation;

// WKNavigation which was added the most recently via `setState:forNavigation:`.
// Updating navigation state via `setState:forNavigation:` does not change the
// last added navigation. Returns nil if there are no stored navigations or
// last navigation was null.
- (WKNavigation*)lastAddedNavigation;

// WKNavigation which was added the most recently via `setState:forNavigation:`
// and has associated navigation context with pending item.
- (WKNavigation*)lastNavigationWithPendingItemInNavigationContext;

// State of WKNavigation which was added the most recently via
// `setState:forNavigation:`. WKNavigationState::NONE if CRWWKNavigationStates
// is empty.
- (web::WKNavigationState)lastAddedNavigationState;

// Returns navigations that are not yet committed, finished or failed.
// This array may contain NSNull to represent null WKNavigation.
- (NSSet*)pendingNavigations;

// webView:didCommitNavigation: can be called multiple times.
// webView:didFinishNavigation: can be called before
// webView:didCommitNavigation:. This method returns YES if the given navigation
// has ever been committed.
- (BOOL)isCommittedNavigation:(WKNavigation*)navigation;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_WK_NAVIGATION_STATES_H_
