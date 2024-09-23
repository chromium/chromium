// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/installedapp/fetch_related_apps_task.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-forward.h"

namespace content {

class RenderFrameHost;
class InstalledAppProviderImplTest;
#if BUILDFLAG(IS_WIN)
class NativeWinAppFetcher;
#endif  // BUILDFLAG(IS_WIN)

class CONTENT_EXPORT InstalledAppProviderImpl
    : public DocumentService<blink::mojom::InstalledAppProvider> {
 public:
  static void Create(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver);

 private:
  // InstalledAppProvider overrides:
  void FilterInstalledApps(
      std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
      const GURL& manifest_url,
      FilterInstalledAppsCallback callback) override;

  void StartTask(std::unique_ptr<FetchRelatedAppsTask> task,
                 std::vector<blink::mojom::RelatedApplicationPtr>& related_apps,
                 FetchRelatedAppsTaskCallback callback);
  void EraseTask(FetchRelatedAppsTask* task);

  void AggregateTaskResults(
      FilterInstalledAppsCallback callback,
      std::vector<FetchRelatedAppsTaskResult> task_result_list);

#if BUILDFLAG(IS_WIN)
  void SetNativeWinAppFetcherFactoryForTesting(
      base::RepeatingCallback<std::unique_ptr<NativeWinAppFetcher>()> factory);
#endif  // BUILDFLAG(IS_WIN)

  explicit InstalledAppProviderImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider>
          pending_receiver);
  ~InstalledAppProviderImpl() override;

  // Used to test the class directly, be careful to destroy object on tear down.
  static InstalledAppProviderImpl* CreateForTesting(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver);
  friend class InstalledAppProviderImplTest;

#if BUILDFLAG(IS_WIN)
  base::RepeatingCallback<std::unique_ptr<NativeWinAppFetcher>()>
      native_win_app_fetcher_factory_;
#endif  // BUILDFLAG(IS_WIN)
  base::flat_map<FetchRelatedAppsTask*, std::unique_ptr<FetchRelatedAppsTask>>
      tasks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<InstalledAppProviderImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_INSTALLED_APP_PROVIDER_IMPL_H_
