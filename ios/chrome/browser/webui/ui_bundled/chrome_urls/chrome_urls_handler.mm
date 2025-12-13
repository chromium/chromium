// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/chrome_urls/chrome_urls_handler.h"

#import <vector>

#import "base/feature_list.h"
#import "base/strings/strcat.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#import "components/prefs/pref_service.h"
#import "components/webui/chrome_urls/pref_names.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "url/gurl.h"

namespace chrome_urls {

namespace {
bool IsWebUIInternal(std::string_view host) {
  return host == commerce::kChromeUICommerceInternalsHost ||
         host == optimization_guide_internals::
                     kChromeUIOptimizationGuideInternalsHost ||
         host == kChromeUIDownloadInternalsHost ||
         host == kChromeUIInterstitialsHost || host == kChromeUILocalStateHost;
}
}  // namespace

ChromeUrlsHandler::ChromeUrlsHandler(
    mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver,
    mojo::PendingRemote<chrome_urls::mojom::Page> page,
    raw_ptr<ProfileIOS> profile)
    : profile_(profile),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {}

ChromeUrlsHandler::~ChromeUrlsHandler() = default;

void ChromeUrlsHandler::GetUrls(GetUrlsCallback callback) {
  std::vector<chrome_urls::mojom::WebuiUrlInfoPtr> webui_urls;
  webui_urls.reserve(kChromeHostURLs.size());
  for (const std::string_view host : kChromeHostURLs) {
    GURL url(
        base::StrCat({kChromeUIScheme, url::kStandardSchemeSeparator, host}));
    chrome_urls::mojom::WebuiUrlInfoPtr url_info(
        chrome_urls::mojom::WebuiUrlInfo::New());
    url_info->url = url;
    url_info->enabled = host != kChromeUINewTabHost;
    url_info->internal = IsWebUIInternal(host);
    webui_urls.push_back(std::move(url_info));
  }

  chrome_urls::mojom::ChromeUrlsDataPtr result(
      chrome_urls::mojom::ChromeUrlsData::New());
  result->webui_urls = std::move(webui_urls);
  result->internal_debugging_uis_enabled =
      GetApplicationContext()->GetLocalState()->GetBoolean(
          chrome_urls::kInternalOnlyUisEnabled);
  std::move(callback).Run(std::move(result));
}

void ChromeUrlsHandler::SetDebugPagesEnabled(
    bool enabled,
    SetDebugPagesEnabledCallback callback) {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      chrome_urls::kInternalOnlyUisEnabled, enabled);
  std::move(callback).Run();
}

}  // namespace chrome_urls
