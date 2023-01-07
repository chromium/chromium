// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_URL_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_URL_HANDLER_H_

#include <vector>

#include "content/common/content_export.h"

class GURL;

namespace content {
class BrowserContext;

// We handle some special browser-level URLs (like "about:version")
// before they're handed to a renderer.  This lets us do the URL handling
// on the browser side (which has access to more information than the
// renderers do) as well as sidestep the risk of exposing data to
// random web pages (because from the resource loader's perspective, these
// URL schemes don't exist).
// BrowserURLHandler manages the list of all special URLs and manages
// dispatching the URL handling to registered handlers.
class CONTENT_EXPORT BrowserURLHandler {
 public:
  // The type of functions that can process a URL.
  // If a handler handles |url|, it should :
  // - optionally modify |url| to the URL that should be sent to the renderer
  // If the URL is not handled by a handler, it should return false.
  typedef bool (*URLHandler)(GURL* url,
                             BrowserContext* browser_context);

  // Returns the null handler for use with |AddHandlerPair()|.
  static URLHandler null_handler();

  // Returns the singleton instance.
  static BrowserURLHandler* GetInstance();

  // RewriteURLIfNecessary gives all registered URLHandlers a shot at processing
  // the given URL, and modifies it in place.
  virtual void RewriteURLIfNecessary(GURL* url,
                                     BrowserContext* browser_context) = 0;

  // Returns the list of possible rewrites, in order of priority (i.e., index 0
  // is the rewrite that would be used in RewriteURLIfNecessary()). Note that
  // this only allows for one rewrite per registered URLHandler (and each gets a
  // fresh copy of |url|), so it is not necessarily the complete set of all
  // possible rewrites.
  virtual std::vector<GURL> GetPossibleRewrites(
      const GURL& url,
      BrowserContext* browser_context) = 0;

  // Add the specified handler pair to the list of URL handlers.
  //
  // Note that normally, the reverse handler is only used if the modified URL is
  // not modified, e.g., by adding a hash fragment. To support this behavior,
  // register the forward and reverse handlers separately, each with a
  // null_handler() for the opposite direction.
  virtual void AddHandlerPair(URLHandler handler,
                              URLHandler reverse_handler) = 0;

 protected:
  virtual ~BrowserURLHandler() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_URL_HANDLER_H_
