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
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  bool IsSharedStorageAllowed(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* rfh,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message = nullptr,
      bool* out_block_is_site_setting_specific = nullptr) override;
  bool IsSharedStorageSelectURLAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message = nullptr,
      bool* out_block_is_site_setting_specific = nullptr) override;
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_INIT_IOS_CONTENT_BROWSER_CLIENT_H_
