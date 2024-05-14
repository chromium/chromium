// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_TEST_INSTALLED_APP_PROVIDER_IMPL_TEST_UTILS_H_
#define CONTENT_BROWSER_INSTALLEDAPP_TEST_INSTALLED_APP_PROVIDER_IMPL_TEST_UTILS_H_

#include <optional>
#include <vector>

#include "content/public/browser/content_browser_client.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-forward.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/installedapp/fetch_related_win_apps_task.h"
#include "content/browser/installedapp/native_win_app_fetcher.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

blink::mojom::RelatedApplicationPtr CreateRelatedApplicationFromPlatformAndId(
    const std::string& platform,
    const std::string& id);

#if BUILDFLAG(IS_WIN)
class FakeNativeWinAppFetcher : public NativeWinAppFetcher {
 public:
  explicit FakeNativeWinAppFetcher(std::vector<std::string> installed_app_ids);
  ~FakeNativeWinAppFetcher() override;

  void FetchAppsForUrl(
      const GURL& url,
      base::OnceCallback<void(std::vector<blink::mojom::RelatedApplicationPtr>)>
          callback) override;

 private:
  std::vector<std::string> installed_app_ids_;
};

std::unique_ptr<NativeWinAppFetcher> CreateFakeNativeWinAppFetcherForTesting(
    std::vector<std::string> installed_win_app_ids_);
#endif  // BUILDFLAG(IS_WIN)

class FakeContentBrowserClientForQueryInstalledWebApps
    : public ContentBrowserClient {
 public:
  explicit FakeContentBrowserClientForQueryInstalledWebApps(
      std::vector<std::string> installed_web_app_ids);
  ~FakeContentBrowserClientForQueryInstalledWebApps() override;

#if !BUILDFLAG(IS_ANDROID)
  void QueryInstalledWebAppsByManifestId(
      const GURL&,
      const GURL& id,
      content::BrowserContext*,
      base::OnceCallback<void(std::optional<blink::mojom::RelatedApplication>)>
          callback) override;
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  std::vector<GURL> installed_web_app_ids_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_TEST_INSTALLED_APP_PROVIDER_IMPL_TEST_UTILS_H_
