// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/test_shared_storage_observer.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TestSharedStorageObserver::TestSharedStorageObserver() = default;
TestSharedStorageObserver::~TestSharedStorageObserver() = default;

void TestSharedStorageObserver::OnSharedStorageAccessed(
    const base::Time& access_time,
    AccessType type,
    FrameTreeNodeId main_frame_id,
    const std::string& owner_origin,
    const SharedStorageEventParams& params) {
  accesses_.emplace_back(type, main_frame_id, owner_origin, params);
}

void TestSharedStorageObserver::OnUrnUuidGenerated(const GURL& urn_uuid) {}

void TestSharedStorageObserver::OnConfigPopulated(
    const std::optional<FencedFrameConfig>& config) {}


void TestSharedStorageObserver::ExpectAccessObserved(
    const std::vector<Access>& expected_accesses) {
  ASSERT_EQ(expected_accesses.size(), accesses_.size());
  for (size_t i = 0; i < accesses_.size(); ++i) {
    EXPECT_EQ(expected_accesses[i], accesses_[i]);
    if (expected_accesses[i] != accesses_[i]) {
      LOG(ERROR) << "Event access differs at index " << i;
    }
  }
}

}  // namespace content
