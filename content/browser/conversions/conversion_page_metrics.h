// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_PAGE_METRICS_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_PAGE_METRICS_H_

namespace content {

class StorableConversion;

// Keeps track of per-page-load metrics for conversion measurement. Lifetime is
// scoped to a single page load.
class ConversionPageMetrics {
 public:
  ConversionPageMetrics();
  ~ConversionPageMetrics();

  ConversionPageMetrics(const ConversionPageMetrics& other) = delete;
  ConversionPageMetrics& operator=(const ConversionPageMetrics& other) = delete;

  // Called when a conversion is registered.
  void OnConversion(const StorableConversion& conversion);

  // Called when an impression is registered.
  void OnImpression();

 private:
  // Keeps track of how many conversion registrations there have been on the
  // current page.
  int num_conversions_on_current_page_ = 0;

  // Keeps track of how many impression registrations there have been on the
  // current page.
  int num_impressions_on_current_page_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_PAGE_METRICS_H_
