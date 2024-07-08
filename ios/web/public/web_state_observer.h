// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_OBSERVER_H_

#include <Foundation/Foundation.h>

#include <stddef.h>

#include <string>
#include <vector>

#include "base/observer_list_types.h"

namespace web {

struct FaviconURL;
class NavigationContext;
enum Permission : NSUInteger;
class WebState;

enum class PageLoadCompletionStatus : bool { SUCCESS = 0, FAILURE = 1 };

// An observer API implemented by classes which are interested in various page
// load events from WebState.
class WebStateObserver : public base::CheckedObserver {
 public:
  WebStateObserver(const WebStateObserver&) = delete;
  WebStateObserver& operator=(const WebStateObserver&) = delete;

  ~WebStateObserver() override;

  // These methods are invoked every time the WebState changes visibility.
  virtual void WasShown(WebState* web_state) {}
  virtual void WasHidden(WebState* web_state) {}

  // Called when a navigation started in the WebState for the main frame.
  // `navigation_context` is unique to a specific navigation. The same
  // NavigationContext will be provided on subsequent call to
  // DidFinishNavigation() when related to this navigation. Observers should
  // clear any references to `navigation_context` in DidFinishNavigation(), just
  // before it is destroyed.
  //
  // This is also fired by same-document navigations, such as fragment
  // navigations or pushState/replaceState, which will not result in a document
  // change. To filter these out, use NavigationContext::IsSameDocument().
  //
  // More than one navigation can be ongoing in the same frame at the same
  // time. Each will get its own NavigationContext.
  //
  // There is no guarantee that DidFinishNavigation() will be called for any
  // particular navigation before DidStartNavigation is called on the next.
  virtual void DidStartNavigation(WebState* web_state,
                                  NavigationContext* navigation_context) {}

  // Called when an in-progress main-frame navigation in `web_state` receives
  // a server redirect to a different URL. At the point where this is called,
  // `navigation_context`'s URL has already been updated, so calling GetUrl()
  // on `navigation_context` will return the redirect URL rather than the
  // original URL.
  virtual void DidRedirectNavigation(WebState* web_state,
                                     NavigationContext* navigation_context) {}

  // Called when a navigation finished in the WebState for the main frame. This
  // happens when a navigation is committed, aborted or replaced by a new one.
  // To know if the navigation has resulted in an error page, use
  // NavigationContext::GetError().
  //
  // If this is called because the navigation committed, then the document load
  // will still be ongoing in the WebState returned by `navigation_context`.
  // Use the document loads events such as DidStopLoading
  // and related methods to listen for continued events from this
  // WebState.
  //
  // This is also fired by same-document navigations, such as fragment
  // navigations or pushState/replaceState, which will not result in a document
  // change. To filter these out, use NavigationContext::IsSameDocument().
  //
  // `navigation_context` will be destroyed at the end of this call, so do not
  // keep a reference to it afterward.
  virtual void DidFinishNavigation(WebState* web_state,
                                   NavigationContext* navigation_context) {}

  // Called when the current WebState has started or stopped loading. This is
  // not correlated with the document load phase of the main frame, but rather
  // represents the load of the web page as a whole. Clients should present
  // network activity indicator UI to the user when DidStartLoading is called
  // and UI when DidStopLoading is called. DidStartLoading is a different event
  // than DidStartNavigation and clients should not assume that these two
  // callbacks always called in pair or in a specific order (same true for
  // DidFinishNavigation/DidFinishLoading). "Navigation" is about fetching the
  // new document content and committing it as a new document, and "Loading"
  // continues well after that. "Loading" callbacks are not called for fragment
  // change navigations, but called for other same-document navigations
  // (crbug.com/767092).
  virtual void DidStartLoading(WebState* web_state) {}
  virtual void DidStopLoading(WebState* web_state) {}

  // Called when the current page has finished the loading of the main frame
  // document (including same-document navigations). DidStopLoading relates to
  // the general loading state of the WebState, but PageLoaded is correlated
  // with the main frame document load phase. Unlike DidStopLoading, this
  // callback is not called when the load is aborted (WebState::Stop is called
  // or the load is rejected via WebStatePolicyDecider (both ShouldAllowRequest
  // or ShouldAllowResponse). If PageLoaded is called it is always called after
  // DidFinishNavigation.
  virtual void PageLoaded(WebState* web_state,
                          PageLoadCompletionStatus load_completion_status) {}

  // Notifies the observer that the page has made some progress loading.
  // `progress` is a value between 0.0 (nothing loaded) to 1.0 (page fully
  // loaded).
  virtual void LoadProgressChanged(WebState* web_state, double progress) {}

  // Called when the canGoBack / canGoForward state of the window was changed.
  virtual void DidChangeBackForwardState(WebState* web_state) {}

  // Called when the title of the WebState is set.
  virtual void TitleWasSet(WebState* web_state) {}

  // Called when the visible security state of the page changes.
  virtual void DidChangeVisibleSecurityState(WebState* web_state) {}

  // Invoked when new favicon URL candidates are received.
  virtual void FaviconUrlUpdated(WebState* web_state,
                                 const std::vector<FaviconURL>& candidates) {}

  // Invoked when the state of a certain permission has changed.
  virtual void PermissionStateChanged(WebState* web_state,
                                      Permission permission) {}

  // Invoked when the under page background color of the WebState has changed.
  virtual void UnderPageBackgroundColorChanged(WebState* web_state) {}

  // Called when the web process is terminated (usually by crashing, though
  // possibly by other means).
  virtual void RenderProcessGone(WebState* web_state) {}

  // Invoked when the WebState becomes realized (e.g. when it becomes fully
  // operational after being restored).
  virtual void WebStateRealized(WebState* web_state) {}

  // Invoked when the WebState is being destroyed. Gives subclasses a chance
  // to cleanup.
  virtual void WebStateDestroyed(WebState* web_state) {}

 protected:
  WebStateObserver();
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_OBSERVER_H_
