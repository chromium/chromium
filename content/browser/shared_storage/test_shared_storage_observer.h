// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_OBSERVER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_OBSERVER_H_

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/time/time.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "url/gurl.h"

namespace content {

class TestSharedStorageObserver
    : public SharedStorageRuntimeManager::SharedStorageObserverInterface {
 public:
  using Access = std::
      tuple<AccessType, FrameTreeNodeId, std::string, SharedStorageEventParams>;

  TestSharedStorageObserver();
  ~TestSharedStorageObserver() override;

  void OnSharedStorageAccessed(const base::Time& access_time,
                               AccessType type,
                               FrameTreeNodeId main_frame_id,
                               const std::string& owner_origin,
                               const SharedStorageEventParams& params) override;

  void OnUrnUuidGenerated(const GURL& urn_uuid) override;

  void OnConfigPopulated(
      const std::optional<FencedFrameConfig>& config) override;

  bool EventParamsMatch(const SharedStorageEventParams& expected_params,
                        const SharedStorageEventParams& actual_params);

  bool AccessesMatch(const Access& expected_access,
                     const Access& actual_access);

  void ExpectAccessObserved(const std::vector<Access>& expected_accesses);

 private:
  std::vector<Access> accesses_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_OBSERVER_H_
