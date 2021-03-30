// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_DELEGATE_IMPL_H_

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/common/content_export.h"

namespace content {

// Implementation of the storage delegate. This class handles assigning
// attribution credits and report times to newly created conversion reports. It
// also controls constants for ConversionStorage. This is owned by
// ConversionStorageSql, and should only be accessed on the conversions storage
// task runner.
class CONTENT_EXPORT ConversionStorageDelegateImpl
    : public ConversionStorage::Delegate {
 public:
  explicit ConversionStorageDelegateImpl(bool debug_mode = false);
  ConversionStorageDelegateImpl(const ConversionStorageDelegateImpl& other) =
      delete;
  ConversionStorageDelegateImpl& operator=(
      const ConversionStorageDelegateImpl& other) = delete;
  ~ConversionStorageDelegateImpl() override = default;

  // ConversionStorageDelegate:
  void ProcessNewConversionReports(
      std::vector<ConversionReport>* reports) override;
  int GetMaxConversionsPerImpression() const override;
  int GetMaxImpressionsPerOrigin() const override;
  int GetMaxConversionsPerOrigin() const override;
  RateLimitConfig GetRateLimits() const override;

 private:
  // Get the time a conversion report should be sent, by batching reports into
  // set reporting windows based on their impression time. This strictly delays
  // the time a report will be sent.
  base::Time GetReportTimeForConversion(const ConversionReport& report) const;

  // Whether the API is running in debug mode, meaning that there should be
  // no delays or noise added to reports.
  bool debug_mode_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_DELEGATE_IMPL_H_
