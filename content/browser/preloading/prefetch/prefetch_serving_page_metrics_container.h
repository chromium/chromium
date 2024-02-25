// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_PAGE_METRICS_CONTAINER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_PAGE_METRICS_CONTAINER_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/prefetch_metrics.h"

namespace content {

// Holds an instance |PrefetchServingPageMetrics| for its associated
// |NavigationHandle|.
class CONTENT_EXPORT PrefetchServingPageMetricsContainer
    : public NavigationHandleUserData<PrefetchServingPageMetricsContainer> {
 public:
  ~PrefetchServingPageMetricsContainer() override;

  PrefetchServingPageMetricsContainer(
      const PrefetchServingPageMetricsContainer&) = delete;
  PrefetchServingPageMetricsContainer& operator=(
      const PrefetchServingPageMetricsContainer&) = delete;

  // Setters that set the metrics in |serving_page_metrics_|.
  void SetPrefetchStatus(PrefetchStatus prefetch_status);
  void SetRequiredPrivatePrefetchProxy(bool required_private_prefetch_proxy);
  void SetPrefetchHeaderLatency(
      const std::optional<base::TimeDelta>& prefetch_header_latency);
  void SetProbeLatency(const base::TimeDelta& probe_latency);

  PrefetchServingPageMetrics& GetServingPageMetrics() {
    return serving_page_metrics_;
  }

  base::WeakPtr<PrefetchServingPageMetricsContainer> GetWeakPtr() {
    return weak_method_factory_.GetWeakPtr();
  }

 private:
  explicit PrefetchServingPageMetricsContainer(
      NavigationHandle& navigation_handle);
  friend NavigationHandleUserData;

  // The metrics related to the prefetch being used for the page being navigated
  // to.
  PrefetchServingPageMetrics serving_page_metrics_;

  base::WeakPtrFactory<PrefetchServingPageMetricsContainer>
      weak_method_factory_{this};

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_PAGE_METRICS_CONTAINER_H_
