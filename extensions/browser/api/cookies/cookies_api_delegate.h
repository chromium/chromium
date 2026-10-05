// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_COOKIES_COOKIES_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_COOKIES_COOKIES_API_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ref.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Provides the chrome.cookies API with embedder-specific hooks for
// functionality that //content does not model generically: the relationship
// between a BrowserContext and its associated off-the-record BrowserContext
// (e.g. a Chrome Profile and its incognito Profile).
class CookiesApiDelegate {
 public:
  virtual ~CookiesApiDelegate() = default;

  // A cookie store's BrowserContext together with the ids of tabs open in
  // windows backed by it, for use by chrome.cookies.getAllCookieStores().
  struct CookieStoreContext {
    raw_ref<content::BrowserContext> browser_context;
    std::vector<int> tab_ids;
  };

  // Returns the cookie stores that currently have at least one open tab:
  // the original context backing `context`, and -- if `include_incognito`
  // and one exists -- its associated off-the-record context.
  virtual std::vector<CookieStoreContext> GetCookieStoreContexts(
      content::BrowserContext& context,
      bool include_incognito) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_COOKIES_COOKIES_API_DELEGATE_H_
