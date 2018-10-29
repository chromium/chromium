// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_URL_HANDLER_IMPL_H_
#define CONTENT_BROWSER_BROWSER_URL_HANDLER_IMPL_H_

#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/public/browser/browser_url_handler.h"

class GURL;

namespace content {
class BrowserContext;

class CONTENT_EXPORT BrowserURLHandlerImpl : public BrowserURLHandler {
 public:
  // Returns the singleton instance.
  static BrowserURLHandlerImpl* GetInstance();

  // BrowserURLHandler implementation:
  void RewriteURLIfNecessary(GURL* url,
                             BrowserContext* browser_context,
                             bool* reverse_on_redirect) override;
  void SetFixupHandler(URLHandler handler) override;
  // Add the specified handler pair to the list of URL handlers.
  void AddHandlerPair(URLHandler handler, URLHandler reverse_handler) override;

  // Fixes up the URL before rewriting occurs.
  void FixupURLBeforeRewrite(GURL* url, BrowserContext* browser_context);

  // Reverses the rewriting that was done for |original| using the new |url|.
  bool ReverseURLRewrite(GURL* url, const GURL& original,
                         BrowserContext* browser_context);

  // Sets the fixup handler during tests. Unlike |SetFixupHandler|, this can be
  // called multiple time during tests.
  void SetFixupHandlerForTesting(URLHandler handler);

 private:
  // This object is a singleton:
  BrowserURLHandlerImpl();
  ~BrowserURLHandlerImpl() override;
  friend struct base::DefaultSingletonTraits<BrowserURLHandlerImpl>;

  // A URLHandler to run in a preliminary phase, before rewriting is done.
  URLHandler fixup_handler_;

  // The list of known URLHandlers, optionally with reverse-rewriters.
  typedef std::pair<URLHandler, URLHandler> HandlerPair;
  std::vector<HandlerPair> url_handlers_;

  FRIEND_TEST_ALL_PREFIXES(BrowserURLHandlerImplTest, BasicRewriteAndReverse);
  FRIEND_TEST_ALL_PREFIXES(BrowserURLHandlerImplTest, NullHandlerReverse);
  FRIEND_TEST_ALL_PREFIXES(BrowserURLHandlerImplTest, ViewSourceReverse);

  DISALLOW_COPY_AND_ASSIGN(BrowserURLHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_URL_HANDLER_IMPL_H_
