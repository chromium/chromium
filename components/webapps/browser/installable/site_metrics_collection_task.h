// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_SITE_METRICS_COLLECTION_TASK_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_SITE_METRICS_COLLECTION_TASK_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"

namespace content {
class WebContents;
}

namespace webapps {

// This class is responsible for gathing metrics for the given site on the given
// web contents to emit the "Site.Manifest" and "Site.Quality" UKM events. After
// emitting the event, the completion closure is called. To stop collection,
// simply destroy this object.
//
// Invariants:
// - `WebContents` is alive during its lifetime of this class.
// - `WebContents` is not navigated during the lifetime of this class and the
//   metrics gathered from it are valid for the `GetLastCommittedURL()`
//   retrieved on construction of this class.
//
// Browsertests are located in
// chrome/browser/web_applications/ml_promotion_browsertest.cc
class SiteMetricsCollectionTask : public content::WebContentsObserver {
 public:
  ~SiteMetricsCollectionTask() override;

  // Creates and starts the metrics collection. The `maximum_wait_time` is used
  // to wait for favicons, manifests, or workers to be added for the given site.
  static std::unique_ptr<SiteMetricsCollectionTask> CreateAndStart(
      content::WebContents& web_contents,
      base::TimeDelta maximum_wait_time,
      base::OnceClosure on_complete);

 private:
  SiteMetricsCollectionTask(content::WebContents& web_contents,
                            base::TimeDelta maximum_wait_time,
                            base::OnceClosure on_complete);

  void Start();

  void OnQuotaRetrieved(blink::mojom::QuotaStatusCode,
                        int64_t usage,
                        int64_t quota,
                        blink::mojom::UsageBreakdownPtr usage_breakdown);

  const GURL site_url_;
  const raw_ref<content::WebContents> web_contents_;
  const base::TimeDelta maximum_wait_time_;
  base::OnceClosure on_complete_;

  // Metrics accumulation.
  int64_t service_worker_script_size = 0;
  int64_t cache_storage_size = 0;

  base::WeakPtrFactory<SiteMetricsCollectionTask> weak_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_SITE_METRICS_COLLECTION_TASK_H_
