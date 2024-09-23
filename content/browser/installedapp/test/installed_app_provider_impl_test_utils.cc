// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/test/installed_app_provider_impl_test_utils.h"

#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

namespace content {

blink::mojom::RelatedApplicationPtr CreateRelatedApplicationFromPlatformAndId(
    const std::string& platform,
    const std::string& id) {
  auto application = blink::mojom::RelatedApplication::New();
  application->platform = platform;
  application->id = id;
  return application;
}

#if BUILDFLAG(IS_WIN)
FakeNativeWinAppFetcher::FakeNativeWinAppFetcher(
    std::vector<std::string> installed_app_ids)
    : installed_app_ids_(installed_app_ids) {}
FakeNativeWinAppFetcher::~FakeNativeWinAppFetcher() = default;

void FakeNativeWinAppFetcher::FetchAppsForUrl(
    const GURL& url,
    base::OnceCallback<void(std::vector<blink::mojom::RelatedApplicationPtr>)>
        callback) {
  std::vector<blink::mojom::RelatedApplicationPtr> related_applications;

  for (const auto& id : installed_app_ids_) {
    related_applications.push_back(
        CreateRelatedApplicationFromPlatformAndId("windows", id));
  }

  std::move(callback).Run(std::move(related_applications));
}

std::unique_ptr<NativeWinAppFetcher> CreateFakeNativeWinAppFetcherForTesting(
    std::vector<std::string> installed_win_app_ids_) {
  return std::make_unique<FakeNativeWinAppFetcher>(
      std::move(installed_win_app_ids_));
}
#endif  // BUILDFLAG(IS_WIN)

FakeContentBrowserClientForQueryInstalledWebApps::
    FakeContentBrowserClientForQueryInstalledWebApps(
        std::vector<std::string> installed_web_app_ids) {
  for (const std::string& id : installed_web_app_ids) {
    GURL id_url(id);
    if (id_url.is_valid()) {
      installed_web_app_ids_.push_back(id_url);
    }
  }
}
FakeContentBrowserClientForQueryInstalledWebApps::
    ~FakeContentBrowserClientForQueryInstalledWebApps() = default;

#if !BUILDFLAG(IS_ANDROID)
void FakeContentBrowserClientForQueryInstalledWebApps::
    QueryInstalledWebAppsByManifestId(
        const GURL&,
        const GURL& id,
        content::BrowserContext*,
        base::OnceCallback<
            void(std::optional<blink::mojom::RelatedApplication>)> callback) {
  std::optional<blink::mojom::RelatedApplication> result;

  if (base::ranges::find(installed_web_app_ids_, id) !=
      installed_web_app_ids_.end()) {
    blink::mojom::RelatedApplication application;
    application.platform = "webapp";
    application.id = id.spec();
    result = application;
  }

  std::move(callback).Run(result);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace content
