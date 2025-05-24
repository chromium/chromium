// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/test_shared_storage_observer.h"

#include <cstddef>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
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

template <typename T>
void ExpectObservations(const std::string& observation_name,
                        const std::vector<T>& expected_observations,
                        const std::vector<T>& actual_observations) {
  ASSERT_EQ(expected_observations.size(), actual_observations.size());
  for (size_t i = 0; i < actual_observations.size(); ++i) {
    EXPECT_EQ(expected_observations[i], actual_observations[i]);
    if (expected_observations[i] != actual_observations[i]) {
      LOG(ERROR) << observation_name << " differs at index " << i;
    }
  }
}

}  // namespace

TestSharedStorageObserver::OperationFinishedInfo::OperationFinishedInfo() =
    default;

TestSharedStorageObserver::OperationFinishedInfo::OperationFinishedInfo(
    base::TimeDelta execution_time,
    AccessMethod method,
    int operation_id,
    int worklet_ordinal_id,
    const base::UnguessableToken& worklet_devtools_token,
    GlobalRenderFrameHostId main_frame_id,
    std::string owner_origin)
    : execution_time(execution_time),
      method(method),
      operation_id(operation_id),
      worklet_ordinal_id(worklet_ordinal_id),
      worklet_devtools_token(worklet_devtools_token),
      main_frame_id(std::move(main_frame_id)),
      owner_origin(std::move(owner_origin)) {}

TestSharedStorageObserver::OperationFinishedInfo::OperationFinishedInfo(
    const TestSharedStorageObserver::OperationFinishedInfo&) = default;
TestSharedStorageObserver::OperationFinishedInfo::OperationFinishedInfo(
    TestSharedStorageObserver::OperationFinishedInfo&&) = default;

TestSharedStorageObserver::OperationFinishedInfo::~OperationFinishedInfo() =
    default;

TestSharedStorageObserver::OperationFinishedInfo&
TestSharedStorageObserver::OperationFinishedInfo::operator=(
    const TestSharedStorageObserver::OperationFinishedInfo&) = default;
TestSharedStorageObserver::OperationFinishedInfo&
TestSharedStorageObserver::OperationFinishedInfo::operator=(
    TestSharedStorageObserver::OperationFinishedInfo&&) = default;

TestSharedStorageObserver::TestSharedStorageObserver() = default;
TestSharedStorageObserver::~TestSharedStorageObserver() = default;

GlobalRenderFrameHostId TestSharedStorageObserver::AssociatedFrameHostId()
    const {
  return GlobalRenderFrameHostId();
}

bool TestSharedStorageObserver::ShouldReceiveAllSharedStorageReports() const {
  return true;
}

void TestSharedStorageObserver::OnSharedStorageAccessed(
    base::Time access_time,
    AccessScope scope,
    AccessMethod method,
    GlobalRenderFrameHostId main_frame_id,
    const std::string& owner_origin,
    const SharedStorageEventParams& params) {
  accesses_.emplace_back(scope, method, main_frame_id, owner_origin, params);
}

void TestSharedStorageObserver::OnSharedStorageSelectUrlUrnUuidGenerated(
    const GURL& urn_uuid) {
  urn_uuids_observed_.push_back(urn_uuid);
}

void TestSharedStorageObserver::OnSharedStorageSelectUrlConfigPopulated(
    const std::optional<FencedFrameConfig>& config) {}

void TestSharedStorageObserver::
    OnSharedStorageWorkletOperationExecutionFinished(
        base::Time finished_time,
        base::TimeDelta execution_time,
        AccessMethod method,
        int operation_id,
        int worklet_ordinal_id,
        const base::UnguessableToken& worklet_devtools_token,
        GlobalRenderFrameHostId main_frame_id,
        const std::string& owner_origin) {
  operation_finished_infos_.emplace_back(
      execution_time, method, operation_id, worklet_ordinal_id,
      worklet_devtools_token, main_frame_id, owner_origin);
}

void TestSharedStorageObserver::ExpectAccessObserved(
    const std::vector<Access>& expected_accesses) {
  ExpectObservations("Event access", expected_accesses, accesses_);
}

void TestSharedStorageObserver::ExpectOperationFinishedInfosObserved(
    const std::vector<OperationFinishedInfo>& expected_infos) {
  ExpectObservations("Operation finished info", expected_infos,
                     operation_finished_infos_);
}

bool operator==(const TestSharedStorageObserver::Access& lhs,
                const TestSharedStorageObserver::Access& rhs) = default;

std::ostream& operator<<(std::ostream& os,
                         const TestSharedStorageObserver::Access& access) {
  os << "{ Access Scope: " << SerializeScope(access.scope)
     << "; Access Method: " << SerializeMethod(access.method)
     << "; Main Frame ID: " << access.main_frame_id
     << "; Owner Origin: " << access.owner_origin
     << "; Params: " << access.params << " }";
  return os;
}

bool operator==(const TestSharedStorageObserver::OperationFinishedInfo& lhs,
                const TestSharedStorageObserver::OperationFinishedInfo& rhs) {
  // Do not compare `execution_time` when checking for equality in tests.
  return lhs.method == rhs.method && lhs.operation_id == rhs.operation_id &&
         lhs.worklet_ordinal_id == rhs.worklet_ordinal_id &&
         lhs.worklet_devtools_token == rhs.worklet_devtools_token &&
         lhs.main_frame_id == rhs.main_frame_id &&
         lhs.owner_origin == rhs.owner_origin;
}

std::ostream& operator<<(
    std::ostream& os,
    const TestSharedStorageObserver::OperationFinishedInfo& info) {
  os << "{ Execution Time: " << info.execution_time.InMicroseconds()
     << "; Access Method: " << SerializeMethod(info.method)
     << "; Operation ID: " << info.operation_id
     << "; Worklet Ordinal ID: " << info.worklet_ordinal_id
     << "; Worklet Devtools Token: " << info.worklet_devtools_token
     << "; Main Frame ID: " << info.main_frame_id
     << "; Owner Origin: " << info.owner_origin << " }";
  return os;
}

}  // namespace content
