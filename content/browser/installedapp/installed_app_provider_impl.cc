// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/installed_app_provider_impl.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "content/browser/browser_thread_impl.h"
#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/installedapp/fetch_related_web_apps_task.h"
#endif  // !BUILDFLAG(IS_ANDROID)
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/installedapp/fetch_related_win_apps_task.h"
#include "content/browser/installedapp/native_win_app_fetcher.h"
#include "content/browser/installedapp/native_win_app_fetcher_impl.h"
#endif

namespace content {

namespace {
constexpr int kMaxNumberOfMatchedApps = 10;

#if BUILDFLAG(IS_WIN)
std::unique_ptr<NativeWinAppFetcher> CreateNativeWinAppFetcher() {
  return std::make_unique<NativeWinAppFetcherImpl>();
}
#endif  // BUILDFLAG(IS_WIN)
}

InstalledAppProviderImpl::InstalledAppProviderImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::InstalledAppProvider> pending_receiver)
    : DocumentService(render_frame_host, std::move(pending_receiver)) {
#if BUILDFLAG(IS_WIN)
  native_win_app_fetcher_factory_ =
      base::BindRepeating(&CreateNativeWinAppFetcher);
#endif  // BUILDFLAG(IS_WIN)
}

InstalledAppProviderImpl::~InstalledAppProviderImpl() = default;

void InstalledAppProviderImpl::FilterInstalledApps(
    std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
    const GURL& manifest_url,
    FilterInstalledAppsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(features::kInstalledAppProvider)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<blink::mojom::RelatedApplicationPtr>()));
    return;
  }

  base::ConcurrentCallbacks<FetchRelatedAppsTaskResult> concurrent;

#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(features::kFilterInstalledAppsWinMatching)) {
    StartTask(std::make_unique<FetchRelatedWinAppsTask>(
                  native_win_app_fetcher_factory_.Run()),
              related_apps, concurrent.CreateCallback());
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kFilterInstalledAppsWebAppMatching)) {
    StartTask(std::make_unique<FetchRelatedWebAppsTask>(
                  render_frame_host().GetBrowserContext()),
              related_apps, concurrent.CreateCallback());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  std::move(concurrent)
      .Done(base::BindOnce(&InstalledAppProviderImpl::AggregateTaskResults,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void InstalledAppProviderImpl::StartTask(
    std::unique_ptr<FetchRelatedAppsTask> task,
    std::vector<blink::mojom::RelatedApplicationPtr>& related_apps,
    FetchRelatedAppsTaskCallback callback) {
  auto* task_ptr = task.get();
  tasks_.insert({task_ptr, std::move(task)});
  auto erase_task_cb = base::BindOnce(&InstalledAppProviderImpl::EraseTask,
                                      weak_ptr_factory_.GetWeakPtr(), task_ptr);
  task_ptr->Start(render_frame_host().GetLastCommittedURL(),
                  mojo::Clone(related_apps),
                  std::move(callback).Then(std::move(erase_task_cb)));
}

void InstalledAppProviderImpl::EraseTask(FetchRelatedAppsTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int num_removed = tasks_.erase(task);
  CHECK(num_removed != 0);
}

void InstalledAppProviderImpl::AggregateTaskResults(
    FilterInstalledAppsCallback callback,
    std::vector<FetchRelatedAppsTaskResult> task_result_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<blink::mojom::RelatedApplicationPtr> matched_apps;

  for (FetchRelatedAppsTaskResult& result : task_result_list) {
    for (blink::mojom::RelatedApplicationPtr& app : result) {
      matched_apps.push_back(std::move(app));
    }
  }

  // |is_off_the_record| should be checked at the end to prevent clients from
  // using timing functions to test if the user is in private.
  bool is_off_the_record =
      render_frame_host().GetProcess()->GetBrowserContext()->IsOffTheRecord();

  if (is_off_the_record) {
    return std::move(callback).Run(
        std::vector<blink::mojom::RelatedApplicationPtr>());
  }

  if (matched_apps.size() > kMaxNumberOfMatchedApps) {
    matched_apps.resize(kMaxNumberOfMatchedApps);
  }
  return std::move(callback).Run(mojo::Clone(matched_apps));
}

#if BUILDFLAG(IS_WIN)
void InstalledAppProviderImpl::SetNativeWinAppFetcherFactoryForTesting(
    base::RepeatingCallback<std::unique_ptr<NativeWinAppFetcher>()> factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_IS_TEST();
  native_win_app_fetcher_factory_ = std::move(factory);
}
#endif  // BUILDFLAG(IS_WIN)

// static
void InstalledAppProviderImpl::Create(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver) {
  if (host.GetParentOrOuterDocument()) {
    // The renderer is supposed to disallow this and we shouldn't end up here.
    mojo::ReportBadMessage(
        "InstalledAppProvider only allowed for outermost main frame.");
    return;
  }

  // The object is bound to the lifetime of |host|'s current document and the
  // mojo connection. See DocumentService for details.
  new InstalledAppProviderImpl(host, std::move(receiver));
}

// static
InstalledAppProviderImpl* InstalledAppProviderImpl::CreateForTesting(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver) {
  CHECK_IS_TEST();
  return new InstalledAppProviderImpl(render_frame_host, std::move(receiver));
}

}  // namespace content
