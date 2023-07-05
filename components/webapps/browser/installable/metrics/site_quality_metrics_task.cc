// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/metrics/site_quality_metrics_task.h"

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webapps {

SiteQualityMetricsTask::~SiteQualityMetricsTask() = default;

// static
std::unique_ptr<SiteQualityMetricsTask> SiteQualityMetricsTask::CreateAndStart(
    const GURL& site_url,
    content::WebContents& web_contents,
    content::StoragePartition& storage_partition,
    content::ServiceWorkerContext& service_worker_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    ResultCallback on_complete) {
  std::unique_ptr<SiteQualityMetricsTask> result =
      base::WrapUnique(new SiteQualityMetricsTask(
          site_url, web_contents, storage_partition, service_worker_context,
          task_runner, std::move(on_complete)));
  result->Start();
  return result;
}

SiteQualityMetricsTask::SiteQualityMetricsTask(
    const GURL& site_url,
    content::WebContents& web_contents,
    content::StoragePartition& storage_partition,
    content::ServiceWorkerContext& service_worker_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    ResultCallback on_complete)
    : site_url_(site_url),
      web_contents_(web_contents),
      storage_partition_(storage_partition),
      service_worker_context_(service_worker_context),
      sequenced_task_runner_(task_runner),
      on_complete_and_self_destruct_(std::move(on_complete)) {
  CHECK(site_url_.is_valid());
  CHECK(!url::Origin::Create(site_url_).opaque());
}

void SiteQualityMetricsTask::Start() {
  auto barrier = base::BarrierClosure(
      /*num_closures=*/2,
      base::BindOnce(&SiteQualityMetricsTask::ReportResultAndSelfDestruct,
                     weak_factory_.GetWeakPtr()));

  blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(site_url_));

  // Quota.
  CHECK(storage_partition_->GetQuotaManager());

  storage_partition_->GetQuotaManager()
      ->proxy()
      ->GetStorageKeyUsageWithBreakdown(
          storage_key, blink::mojom::StorageType::kTemporary,
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&SiteQualityMetricsTask::OnQuotaUsageRetrieved,
                         weak_factory_.GetWeakPtr())
              .Then(barrier));

  // Service worker.
  service_worker_context_->CheckHasServiceWorker(
      site_url_, storage_key,
      base::BindOnce(&SiteQualityMetricsTask::OnDidCheckHasServiceWorker,
                     weak_factory_.GetWeakPtr())
          .Then(barrier));
}

void SiteQualityMetricsTask::OnQuotaUsageRetrieved(
    int64_t usage,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  if (!usage_breakdown) {
    return;
  }

  service_worker_script_size_ = usage_breakdown->serviceWorker;
  cache_storage_size_ = usage_breakdown->serviceWorkerCache;
}

void SiteQualityMetricsTask::OnDidCheckHasServiceWorker(
    content::ServiceWorkerCapability capability) {
  switch (capability) {
    case content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER:
      has_service_worker_ = true;
      has_fetch_handler_ = true;
      break;
    case content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER:
      has_service_worker_ = true;
      has_fetch_handler_ = false;
      break;
    case content::ServiceWorkerCapability::NO_SERVICE_WORKER:
      has_service_worker_ = false;
      has_fetch_handler_ = false;
      break;
  }
}

void SiteQualityMetricsTask::ReportResultAndSelfDestruct() {
  // Only count favicon URLs that are not the default one set by the renderer in
  // the absence of icons in the html. Default URLs follow the
  // <document_origin>/favicon.ico format.
  for (const auto& favicon_urls : web_contents_->GetFaviconURLs()) {
    if (!favicon_urls->is_default_icon) {
      non_default_favicon_count_++;
    }
  }

  std::move(on_complete_and_self_destruct_)
      .Run(SiteQualityMetrics(service_worker_script_size_, cache_storage_size_,
                              non_default_favicon_count_, has_service_worker_,
                              has_fetch_handler_));
}

}  // namespace webapps
