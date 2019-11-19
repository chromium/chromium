// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_CACHED_NAVIGATION_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_CACHED_NAVIGATION_URL_LOADER_H_

#include "content/browser/loader/navigation_url_loader.h"

namespace content {

// NavigationURLLoader for navigations served by the back-forward cache. Unlike
// a normal navigation, no actual URL loading occurs. This is because loading
// already happened the first time this URL was navigated to (before it was
// added to the back-forward cache).
class CachedNavigationURLLoader : public NavigationURLLoader {
 public:
  CachedNavigationURLLoader(std::unique_ptr<NavigationRequestInfo> request_info,
                            NavigationURLLoaderDelegate* delegate);
  ~CachedNavigationURLLoader() override;

  static std::unique_ptr<NavigationURLLoader> Create(
      std::unique_ptr<NavigationRequestInfo> request_info,
      NavigationURLLoaderDelegate* delegate);

  // NavigationURLLoader implementation.
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      PreviewsState new_previews_state) override;

 private:
  void OnResponseStarted();
  std::unique_ptr<NavigationRequestInfo> request_info_;
  NavigationURLLoaderDelegate* delegate_;
  base::WeakPtrFactory<CachedNavigationURLLoader> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_CACHED_NAVIGATION_URL_LOADER_H_
