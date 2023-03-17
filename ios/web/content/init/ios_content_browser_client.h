// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_INIT_IOS_CONTENT_BROWSER_CLIENT_H_
#define IOS_WEB_CONTENT_INIT_IOS_CONTENT_BROWSER_CLIENT_H_

#import "build/blink_buildflags.h"
#include "content/public/browser/content_browser_client.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace web {

class IOSContentBrowserClient : public content::ContentBrowserClient {
 public:
  // ContentBrowserClient implementation:
  bool IsHandledURL(const GURL& url) override;
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  std::string GetProduct() override;
  std::string GetUserAgent() override;
  std::string GetUserAgentBasedOnPolicy(
      content::BrowserContext* context) override;
  std::string GetFullUserAgent() override;
  std::string GetReducedUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  bool IsSharedStorageAllowed(content::BrowserContext* browser_context,
                              content::RenderFrameHost* rfh,
                              const url::Origin& top_frame_origin,
                              const url::Origin& accessing_origin) override;
  bool IsSharedStorageSelectURLAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin) override;
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_INIT_IOS_CONTENT_BROWSER_CLIENT_H_
