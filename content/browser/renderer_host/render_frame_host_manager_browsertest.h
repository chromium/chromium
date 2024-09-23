// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_MANAGER_BROWSERTEST_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_MANAGER_BROWSERTEST_H_

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/url_loader_interceptor.h"

namespace content {

class RenderFrameHostManagerTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  RenderFrameHostManagerTest();
  ~RenderFrameHostManagerTest() override;

  void SetUpOnMainThread() override;

  void DisableBackForwardCache(
      BackForwardCacheImpl::DisableForTestingReason reason) const;
  void StartServer();
  void StartEmbeddedServer();
  std::unique_ptr<content::URLLoaderInterceptor> SetupRequestFailForURL(
      const GURL& url);

  // Returns a URL on foo.com with the given path.
  GURL GetCrossSiteURL(const std::string& path);

  void NavigateToPageWithLinks(Shell* shell);

 protected:
  void AssertCanRemoveSubframeInPageHide(bool same_site);

  std::string foo_com_;
  GURL::Replacements replace_host_;
  net::HostPortPair foo_host_port_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_MANAGER_BROWSERTEST_H_
