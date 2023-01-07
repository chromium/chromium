// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_ATTRIBUTION_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_ATTRIBUTION_MANAGER_H_

#include "third_party/blink/public/mojom/conversions/attribution_reporting_automation.mojom.h"

#include "base/memory/raw_ptr.h"

namespace content {

class StoragePartition;

// Provides privileged access to a storage partition's `AttributionManager` in
// order to support test-specific behavior.
class WebTestAttributionManager
    : public blink::test::mojom::AttributionReportingAutomation {
 public:
  explicit WebTestAttributionManager(StoragePartition& storage_partition);

  ~WebTestAttributionManager() override = default;

  WebTestAttributionManager(const WebTestAttributionManager&) = delete;

  WebTestAttributionManager& operator=(const WebTestAttributionManager&) =
      delete;

  // blink::test::mojom::AttributionReportingAutomation:
  void Reset(ResetCallback) override;

 private:
  const base::raw_ptr<StoragePartition> storage_partition_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_ATTRIBUTION_MANAGER_H_
