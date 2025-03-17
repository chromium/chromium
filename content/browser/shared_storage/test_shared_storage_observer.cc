// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/test_shared_storage_observer.h"

#include <cstddef>
#include <optional>
#include <ostream>
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

namespace {

using AccessScope = blink::SharedStorageAccessScope;
using AccessMethod = TestSharedStorageObserver::AccessMethod;

std::string SerializeScope(AccessScope scope) {
  switch (scope) {
    case AccessScope::kWindow:
      return "Window";
    case AccessScope::kSharedStorageWorklet:
      return "SharedStorageWorklet";
    case AccessScope::kProtectedAudienceWorklet:
      return "ProtectedAudienceWorklet";
    case AccessScope::kHeader:
      return "Header";
  }
  NOTREACHED();
}

std::string SerializeMethod(AccessMethod method) {
  switch (method) {
    case AccessMethod::kAddModule:
      return "AddModule";
    case AccessMethod::kCreateWorklet:
      return "CreateWorklet";
    case AccessMethod::kSelectURL:
      return "SelectURL";
    case AccessMethod::kRun:
      return "Run";
    case AccessMethod::kBatchUpdate:
      return "BatchUpdate";
    case AccessMethod::kSet:
      return "Set";
    case AccessMethod::kAppend:
      return "Append";
    case AccessMethod::kDelete:
      return "Delete";
    case AccessMethod::kClear:
      return "Clear";
    case AccessMethod::kGet:
      return "Get";
    case AccessMethod::kKeys:
      return "Keys";
    case AccessMethod::kValues:
      return "Values";
    case AccessMethod::kEntries:
      return "Entries";
    case AccessMethod::kLength:
      return "Length";
    case AccessMethod::kRemainingBudget:
      return "RemainingBudget";
  }
  NOTREACHED();
}

}  // namespace

TestSharedStorageObserver::TestSharedStorageObserver() = default;
TestSharedStorageObserver::~TestSharedStorageObserver() = default;

void TestSharedStorageObserver::OnSharedStorageAccessed(
    const base::Time& access_time,
    AccessScope scope,
    AccessMethod method,
    FrameTreeNodeId main_frame_id,
    const std::string& owner_origin,
    const SharedStorageEventParams& params) {
  accesses_.emplace_back(scope, method, main_frame_id, owner_origin, params);
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

bool operator==(const TestSharedStorageObserver::Access& lhs,
                const TestSharedStorageObserver::Access& rhs) = default;

std::ostream& operator<<(std::ostream& os,
                         const TestSharedStorageObserver::Access& access) {
  os << "{ Access Scope: " << SerializeScope(access.scope)
     << "; Access Method: " << SerializeMethod(access.method)
     << "; Main Frame Id: " << access.main_frame_id.GetUnsafeValue()
     << "; Owner Origin: " << access.owner_origin
     << "; Params: " << access.params << " }";
  return os;
}

}  // namespace content
