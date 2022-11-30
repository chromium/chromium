// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_BROWSER_URL_REWRITER_H_
#define IOS_WEB_PUBLIC_NAVIGATION_BROWSER_URL_REWRITER_H_

#include <vector>

class GURL;

namespace web {

class BrowserState;

// Some special browser-level URLs (like "about:version") are handled before
// actually being loaded by the web view, allowing the embedder to optionally
// convert them to app-specific URLs.
class BrowserURLRewriter {
 public:
  // The type of functions that can process a URL.  URLRewriters return true if
  // `url` is ready to be loaded and added to the NavigationManager.  They can
  // optionally modify `url` regardless of whether they return true or false.
  typedef bool (*URLRewriter)(GURL* url, BrowserState* browser_state);

  // Returns the singleton instance.
  static BrowserURLRewriter* GetInstance();

  // Gives every URLRewriter in `rewriters` a chance to process `url`, modifying
  // it in place.  Returns whether or not a URLRewriter returned `true`.
  static bool RewriteURLWithWriters(
      GURL* url,
      BrowserState* browser_state,
      const std::vector<BrowserURLRewriter::URLRewriter>& rewriters);

  // Gives every URLRewriter added via `AddURLRewriter()` a chance to process
  // `url`, modifying it in place.  Returns whether or not a URLRewriter
  // returned `true`.
  virtual bool RewriteURLIfNecessary(GURL* url,
                                     BrowserState* browser_state) = 0;

  // Adds `rewriter` to the list of URL rewriters.  `rewriter` must not be null.
  virtual void AddURLRewriter(URLRewriter rewriter) = 0;

 protected:
  virtual ~BrowserURLRewriter() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_BROWSER_URL_REWRITER_H_
