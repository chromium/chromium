// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_CONTEXT_H_
#define IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_CONTEXT_H_

#import <Foundation/Foundation.h>

#include "ui/base/page_transition_types.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}

namespace web {

class WebState;

// Tracks information related to a single navigation. A NavigationContext is
// provided to WebStateObserver methods to allow observers to track specific
// navigations and their details. Observers should clear any references to a
// NavigationContext at the time of WebStateObserver::DidFinishNavigation, just
// before the handle is destroyed.
class NavigationContext {
 public:
  // The WebState the navigation is taking place in.
  virtual WebState* GetWebState() = 0;

  // Returns a unique ID for this navigation. This number is a counter, so
  // new context will have higher ID number than older context.
  virtual int64_t GetNavigationId() const = 0;

  // The URL the WebState is navigating to. This may change during the
  // navigation when encountering a server redirect.
  virtual const GURL& GetUrl() const = 0;

  // Whether the navigation was initiated by a user gesture. Note that this
  // will return true for browser-initiated navigations (not a
  // renderer-initiated navigation). May have false positives for
  // renderer-initiated same-document or back-forward navigations.
  virtual bool HasUserGesture() const = 0;

  // Returns the page transition type for this navigation.
  virtual ui::PageTransition GetPageTransition() const = 0;

  // Whether the navigation happened within the same document. Examples of same
  // document navigations are:
  // * reference fragment navigations
  // * pushState/replaceState
  // * same document history navigation
  virtual bool IsSameDocument() const = 0;

  // Whether the navigation has committed. Navigations that end up being
  // downloads, return 204/205 response codes or their response is rejected by
  // the policy decider do not commit (i.e. the WebState stays at the existing
  // URL).
  // This returns true for either successful commits or error pages that
  // replace the previous page, and false for errors that leave the user on the
  // previous page.
  virtual bool HasCommitted() const = 0;

  // Returns true if this navigation resulted in a download. Returns false if
  // this navigation did not result in a download, or if download status is not
  // yet known for this navigation.
  virtual bool IsDownload() const = 0;

  // Whether the initial navigation is done using HTTP POST method. This will
  // not change during the navigation (even after encountering a server
  // redirect).
  //
  // Note: page and frame navigations can only be done using POST or GET
  // methods Therefore API exposes only |bool IsPost()| as opposed to
  // |const std::string& GetMethod()| method.
  virtual bool IsPost() const = 0;

  // Returns error if the navigation has failed.
  virtual NSError* GetError() const = 0;

  // Returns the response headers for the request, or null if there aren't any
  // response headers or they have not been received yet. The response headers
  // returned should not be modified, as modifications will not be reflected.
  virtual net::HttpResponseHeaders* GetResponseHeaders() const = 0;

  // Whether the navigation was initiated by the renderer process. Examples of
  // renderer-initiated navigations include:
  //  * <a> link click
  //  * changing window.location.href
  //  * redirect via the <meta http-equiv="refresh"> tag
  //  * using window.history.pushState
  virtual bool IsRendererInitiated() const = 0;

  virtual ~NavigationContext() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_CONTEXT_H_
