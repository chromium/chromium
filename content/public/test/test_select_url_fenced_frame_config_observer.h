// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_SELECT_URL_FENCED_FRAME_CONFIG_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_TEST_SELECT_URL_FENCED_FRAME_CONFIG_OBSERVER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"

class GURL;

namespace content {

class FencedFrameConfig;
class StoragePartition;
class TestSelectURLFencedFrameConfigObserverImpl;

// This observes:
// 1. the next generated urn::uuid.
// 2. the next populated fenced frame config that contains the observed
// urn::uuid.
//
// With the fenced frame API change, `selectURL()` can return a fenced frame
// config object instead of an urn::uuid. The urn::uuid in the returned config
// is opaque, but it is often needed to check the associated info
// (e.g. SharedStorageBudgetMetadata). Tests can use this observer to obtain the
// urn::uuid and the browser-side FencedFrameConfig.
//
// This observes only the first url::uuid and config. To observe a new one, a
// new observer must be created before calling `selectURL().
class TestSelectURLFencedFrameConfigObserver {
 public:
  explicit TestSelectURLFencedFrameConfigObserver(
      StoragePartition* storage_partition);
  ~TestSelectURLFencedFrameConfigObserver();

  const std::optional<GURL>& GetUrnUuid() const;
  const std::optional<FencedFrameConfig>& GetConfig() const;
  bool ConfigObserved() const;

 private:
  raw_ptr<StoragePartition> storage_partition_;
  std::unique_ptr<TestSelectURLFencedFrameConfigObserverImpl> impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_SELECT_URL_FENCED_FRAME_CONFIG_OBSERVER_H_
