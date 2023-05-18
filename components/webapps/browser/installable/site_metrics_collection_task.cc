// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/site_metrics_collection_task.h"
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace webapps {

SiteMetricsCollectionTask::~SiteMetricsCollectionTask() = default;

// static
std::unique_ptr<SiteMetricsCollectionTask>
SiteMetricsCollectionTask::CreateAndStart(content::WebContents& web_contents,
                                          base::TimeDelta maximum_wait_time,
                                          base::OnceClosure on_complete) {
  std::unique_ptr<SiteMetricsCollectionTask> result =
      base::WrapUnique(new SiteMetricsCollectionTask(
          web_contents, maximum_wait_time, std::move(on_complete)));
  result->Start();
  return result;
}

SiteMetricsCollectionTask::SiteMetricsCollectionTask(
    content::WebContents& web_contents,
    base::TimeDelta maximum_wait_time,
    base::OnceClosure on_complete)
    : content::WebContentsObserver(&web_contents),
      site_url_(web_contents.GetLastCommittedURL()),
      web_contents_(web_contents),
      maximum_wait_time_(maximum_wait_time),
      on_complete_(std::move(on_complete)) {
  CHECK(site_url_.is_valid());
  CHECK(maximum_wait_time.is_positive() || maximum_wait_time.is_zero());
}

void SiteMetricsCollectionTask::Start() {
  content::StoragePartition* storage_partition =
      web_contents_->GetPrimaryMainFrame()->GetStoragePartition();
  CHECK(storage_partition);
  CHECK(storage_partition->GetQuotaManager());
  storage_partition->GetQuotaManager()->proxy()->GetUsageAndQuotaWithBreakdown(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(site_url_)),
      blink::mojom::StorageType::kTemporary,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&SiteMetricsCollectionTask::OnQuotaRetrieved,
                     weak_factory_.GetWeakPtr()));
}

void SiteMetricsCollectionTask::OnQuotaRetrieved(
    blink::mojom::QuotaStatusCode code,
    int64_t usage,
    int64_t quota,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  if (code != blink::mojom::QuotaStatusCode::kOk) {
    // TODO(b/279521783): Handle erroneous QuotaStatusCode values and implement
    // filtering before ML is triggered.
    return;
  }

  service_worker_script_size = usage_breakdown->serviceWorker;
  cache_storage_size = usage_breakdown->serviceWorkerCache;

  // TODO(b/279521783): Continue with metrics collection, and eventually emit
  // them as a UKM metric.
}

}  // namespace webapps
