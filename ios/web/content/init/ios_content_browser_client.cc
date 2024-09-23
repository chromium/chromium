// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/content/init/ios_content_browser_client.h"

#import "components/embedder_support/user_agent_utils.h"
#import "components/performance_manager/embedder/performance_manager_registry.h"
#import "components/version_info/version_info.h"
#import "content/public/browser/browser_context.h"
#import "content/public/browser/devtools_manager_delegate.h"
#import "content/public/browser/web_contents_view_delegate.h"
#import "content/public/common/url_constants.h"
#import "content/public/common/user_agent.h"
#import "ios/web/content/init/ios_browser_main_parts.h"
#import "ios/web/content/ui/web_contents_view_delegate_impl.h"

namespace web {

bool IOSContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  static const char* const kProtocolList[] = {
      url::kHttpScheme,
      url::kHttpsScheme,
      url::kWsScheme,
      url::kWssScheme,
      url::kBlobScheme,
      url::kFileSystemScheme,
      content::kChromeUIScheme,
      content::kChromeUIUntrustedScheme,
      content::kChromeDevToolsScheme,
      url::kDataScheme,
      url::kFileScheme,
  };
  for (const char* supported_protocol : kProtocolList) {
    if (url.scheme_piece() == supported_protocol) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<content::BrowserMainParts>
IOSContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  return std::make_unique<IOSBrowserMainParts>();
}

std::string IOSContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return "en-us,en";
}

std::string IOSContentBrowserClient::GetProduct() {
  return std::string(version_info::GetProductNameAndVersionForUserAgent());
}

std::string IOSContentBrowserClient::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

std::string IOSContentBrowserClient::GetUserAgentBasedOnPolicy(
    content::BrowserContext* context) {
  return GetUserAgent();
}

blink::UserAgentMetadata IOSContentBrowserClient::GetUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list.emplace_back(
      std::string(version_info::GetProductName()), "113");
  metadata.brand_full_version_list.emplace_back(
      std::string(version_info::GetProductName()),
      std::string(version_info::GetVersionNumber()));
  metadata.full_version = std::string(version_info::GetVersionNumber());
  metadata.platform = "Unknown";
  metadata.architecture = content::GetCpuArchitecture();
  metadata.model = content::BuildModelInfo();

  metadata.bitness = content::GetCpuBitness();
  metadata.wow64 = content::IsWoW64();

  return metadata;
}

std::unique_ptr<content::WebContentsViewDelegate>
IOSContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  if (auto* registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    registry->MaybeCreatePageNodeForWebContents(web_contents);
  }
  return CreateWebContentsViewDelegate(web_contents);
}

bool IOSContentBrowserClient::IsSharedStorageAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  return true;
}

bool IOSContentBrowserClient::IsSharedStorageSelectURLAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  return true;
}

content::GeneratedCodeCacheSettings
IOSContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return content::GeneratedCodeCacheSettings(true, 0, context->GetPath());
}

std::unique_ptr<content::DevToolsManagerDelegate>
IOSContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<content::DevToolsManagerDelegate>();
}

}  // namespace web
