// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_URL_HANDLER_IMPL_H_
#define CONTENT_BROWSER_BROWSER_URL_HANDLER_IMPL_H_

#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_url_handler.h"

class GURL;

namespace content {
class BrowserContext;

class CONTENT_EXPORT BrowserURLHandlerImpl : public BrowserURLHandler {
 public:
  // Returns the singleton instance.
  static BrowserURLHandlerImpl* GetInstance();

  BrowserURLHandlerImpl(const BrowserURLHandlerImpl&) = delete;
  BrowserURLHandlerImpl& operator=(const BrowserURLHandlerImpl&) = delete;

  // BrowserURLHandler implementation:
  void RewriteURLIfNecessary(GURL* url,
                             BrowserContext* browser_context) override;
  std::vector<GURL> GetPossibleRewrites(
      const GURL& url,
      BrowserContext* browser_context) override;
  void AddHandlerPair(URLHandler handler, URLHandler reverse_handler) override;

  // Like the //content-public RewriteURLIfNecessary overload (overridden
  // above), but if the original URL needs to be adjusted if the modified URL is
  // redirected, this method sets |*reverse_on_redirect| to true.
  void RewriteURLIfNecessary(GURL* url,
                             BrowserContext* browser_context,
                             bool* reverse_on_redirect);

  // Reverses the rewriting that was done for |original| using the new |url|.
  bool ReverseURLRewrite(GURL* url, const GURL& original,
                         BrowserContext* browser_context);

  // Reverses |AddHandlerPair| for the given |handler|.
  void RemoveHandlerForTesting(URLHandler handler);

 private:
  // This object is a singleton:
  BrowserURLHandlerImpl();
  ~BrowserURLHandlerImpl() override;
  friend struct base::DefaultSingletonTraits<BrowserURLHandlerImpl>;

  // The list of known URLHandlers, optionally with reverse-rewriters.
  typedef std::pair<URLHandler, URLHandler> HandlerPair;
  std::vector<HandlerPair> url_handlers_;

  FRIEND_TEST_ALL_PREFIXES(BrowserURLHandlerImplTest, BasicRewriteAndReverse);
  FRIEND_TEST_ALL_PREFIXES(BrowserURLHandlerImplTest, NullHandlerReverse);
  FRIEND_TEST_ALL_PREFIXES(BrowserURLHandlerImplTest, ViewSourceReverse);
  FRIEND_TEST_ALL_PREFIXES(BrowserURLHandlerImplTest, GetPossibleRewrites);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_URL_HANDLER_IMPL_H_
