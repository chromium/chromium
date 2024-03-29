// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_COOKIE_CHECKER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_COOKIE_CHECKER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/common/content_export.h"

namespace content {

class StoragePartitionImpl;

class CONTENT_EXPORT AttributionCookieCheckerImpl
    : public AttributionCookieChecker {
 public:
  explicit AttributionCookieCheckerImpl(StoragePartitionImpl*);

  ~AttributionCookieCheckerImpl() override;

  AttributionCookieCheckerImpl(const AttributionCookieCheckerImpl&) = delete;
  AttributionCookieCheckerImpl(AttributionCookieCheckerImpl&&) = delete;

  AttributionCookieCheckerImpl& operator=(const AttributionCookieCheckerImpl&) =
      delete;
  AttributionCookieCheckerImpl& operator=(AttributionCookieCheckerImpl&&) =
      delete;

  // AttributionCookieChecker:
  void IsDebugCookieSet(const url::Origin&, Callback) override;

 private:
  const raw_ref<StoragePartitionImpl> storage_partition_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_COOKIE_CHECKER_IMPL_H_
