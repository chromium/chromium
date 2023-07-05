// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_METRICS_SITE_QUALITY_METRICS_TASK_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_METRICS_SITE_QUALITY_METRICS_TASK_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"
#include "url/gurl.h"

namespace content {
class WebContents;
enum class ServiceWorkerCapability;
class ServiceWorkerContext;
class StoragePartition;
}  // namespace content

namespace webapps {

// This class is responsible for gathing metrics for the given site on the given
// web contents to emit the "Site.Quality" UKM events. To stop collection,
// simply destroy this object.
//
// Invariants:
// - `WebContents` is alive during its lifetime of this class.
// - `WebContents` is not navigated during the lifetime of this class and the
//   metrics gathered from it are valid for the `GetLastCommittedURL()`
//   retrieved on construction of this class.
// - The default StoragePartition and the ServiceWorkerContext in use is alive
// for the duration of this class.
//
// Browsertests are located in
// chrome/browser/web_applications/ml_promotion_browsertest.cc

struct SiteQualityMetrics {
  SiteQualityMetrics(int64_t service_worker_script_size,
                     int64_t cache_storage_size,
                     size_t non_default_favicons_count,
                     bool has_service_worker,
                     bool has_fetch_handler)
      : service_worker_script_size(service_worker_script_size),
        cache_storage_size(cache_storage_size),
        non_default_favicons_count(non_default_favicons_count),
        has_service_worker(has_service_worker),
        has_fetch_handler(has_fetch_handler) {}
  SiteQualityMetrics() = default;
  ~SiteQualityMetrics() = default;

  int64_t service_worker_script_size = 0;
  int64_t cache_storage_size = 0;
  size_t non_default_favicons_count = 0ul;
  bool has_service_worker = false;
  bool has_fetch_handler = false;
};

// Returns the default favicon URL for the document, mimics behavior of
// blink::IconURL::DefaultFavicon().
GURL GetDefaultFaviconUrl(const GURL& site_url);

class SiteQualityMetricsTask {
 public:
  using ResultCallback = base::OnceCallback<void(const SiteQualityMetrics&)>;
  ~SiteQualityMetricsTask();

  // Creates and starts the metrics collection for Site.Quality events. Ensure
  // that web_contents->GetLastCommittedURL() is not opaque when calling this
  // function.
  static std::unique_ptr<SiteQualityMetricsTask> CreateAndStart(
      const GURL& site_url,
      content::WebContents& web_contents,
      content::StoragePartition& storage_partition,
      content::ServiceWorkerContext& service_worker_context,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ResultCallback on_complete);

 private:
  SiteQualityMetricsTask(const GURL& site_url,
                         content::WebContents& web_contents,
                         content::StoragePartition& storage_partition,
                         content::ServiceWorkerContext& service_worker_context,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         ResultCallback on_complete);

  void Start();
  void OnQuotaUsageRetrieved(int64_t usage,
                             blink::mojom::UsageBreakdownPtr usage_breakdown);

  void OnDidCheckHasServiceWorker(content::ServiceWorkerCapability capability);

  void ReportResultAndSelfDestruct();

  const GURL site_url_;
  const raw_ref<content::WebContents> web_contents_;
  const raw_ref<content::StoragePartition> storage_partition_;
  const raw_ref<content::ServiceWorkerContext> service_worker_context_;

  // This is the base::SequencedTaskRunner::CurrentDefault() on production. In
  // tests this might need to be faked out, which is why we pass this in from
  // the MLInstallabilityPromoter.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  ResultCallback on_complete_and_self_destruct_;

  // Metrics accumulation.
  int64_t service_worker_script_size_ = 0;
  int64_t cache_storage_size_ = 0;
  size_t non_default_favicon_count_ = 0ul;
  bool has_service_worker_ = false;
  bool has_fetch_handler_ = false;

  base::WeakPtrFactory<SiteQualityMetricsTask> weak_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_METRICS_SITE_QUALITY_METRICS_TASK_H_
