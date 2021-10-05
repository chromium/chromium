// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_PAGE_METRICS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_PAGE_METRICS_H_

#include "base/containers/flat_set.h"
#include "url/origin.h"

namespace content {

// Keeps track of per-page-load metrics for conversion measurement. Lifetime is
// scoped to a single page load.
class AttributionPageMetrics {
 public:
  AttributionPageMetrics();
  ~AttributionPageMetrics();

  AttributionPageMetrics(const AttributionPageMetrics& other) = delete;
  AttributionPageMetrics& operator=(const AttributionPageMetrics& other) =
      delete;
  AttributionPageMetrics(AttributionPageMetrics&& other) = delete;
  AttributionPageMetrics& operator=(AttributionPageMetrics&& other) = delete;

  // Called when a conversion is registered.
  void OnConversion(url::Origin reporting_origin);

  // Called when an impression is registered.
  void OnImpression(url::Origin reporting_origin);

 private:
  // Keeps track of how many conversion registrations there have been on the
  // current page.
  int num_conversions_on_current_page_ = 0;

  // Keeps track of how many impression registrations there have been on the
  // current page.
  int num_impressions_on_current_page_ = 0;

  // Keeps track of how many unique reporting origins for conversion
  // registrations there have been on the current page.
  base::flat_set<url::Origin> conversion_reporting_origins_on_current_page_;

  // Keeps track of how many unique reporting origins for impression
  // registrations there have been on the current page. Note that Android app
  // initiated navigation impressions are not included.
  base::flat_set<url::Origin> impression_reporting_origins_on_current_page_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_PAGE_METRICS_H_
