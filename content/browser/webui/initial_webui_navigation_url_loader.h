// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_INITIAL_WEBUI_NAVIGATION_URL_LOADER_H_
#define CONTENT_BROWSER_WEBUI_INITIAL_WEBUI_NAVIGATION_URL_LOADER_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/loader/navigation_url_loader.h"

namespace content {

// NavigationURLLoader for navigations to the initial WebUI. No actual network /
// URL loading occurs, because the HTML body is actually loaded within the
// renderer process.
class InitialWebUINavigationURLLoader : public NavigationURLLoader {
 public:
  InitialWebUINavigationURLLoader(
      BrowserContext* browser_context,
      std::unique_ptr<NavigationRequestInfo> request_info,
      NavigationURLLoaderDelegate* delegate);
  ~InitialWebUINavigationURLLoader() override;

  static std::unique_ptr<NavigationURLLoader> Create(
      BrowserContext* browser_context,
      std::unique_ptr<NavigationRequestInfo> request_info,
      NavigationURLLoaderDelegate* delegate);

  // NavigationURLLoader implementation.
  void Start() override;
  void FollowRedirect(
      std::vector<std::string> removed_headers,
      net::HttpRequestHeaders modified_headers,
      net::HttpRequestHeaders modified_cors_exempt_headers) override;
  bool SetNavigationTimeout(base::TimeDelta timeout) override;
  void CancelNavigationTimeout() override;

 private:
  void OnResponseStarted();

  raw_ptr<BrowserContext> browser_context_;
  std::unique_ptr<NavigationRequestInfo> request_info_;
  raw_ptr<NavigationURLLoaderDelegate> delegate_;
  base::WeakPtrFactory<InitialWebUINavigationURLLoader> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_INITIAL_WEBUI_NAVIGATION_URL_LOADER_H_
