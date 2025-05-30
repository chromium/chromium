// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_OBSERVER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_OBSERVER_H_

#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/time/time.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "url/gurl.h"

namespace content {

class TestSharedStorageObserver
    : public SharedStorageRuntimeManager::SharedStorageObserverInterface {
 public:
  using AccessScope = blink::SharedStorageAccessScope;

  struct Access {
    AccessScope scope;
    AccessMethod method;
    GlobalRenderFrameHostId main_frame_id;
    std::string owner_origin;
    SharedStorageEventParams params;
    friend bool operator==(const Access& lhs, const Access& rhs);
  };

  struct OperationFinishedInfo {
    base::TimeDelta execution_time;
    AccessMethod method;
    int operation_id;
    base::UnguessableToken worklet_devtools_token;
    GlobalRenderFrameHostId main_frame_id;
    std::string owner_origin;
    OperationFinishedInfo();
    OperationFinishedInfo(base::TimeDelta execution_time,
                          AccessMethod method,
                          int operation_id,
                          const base::UnguessableToken& worklet_devtools_token,
                          GlobalRenderFrameHostId main_frame_id,
                          std::string owner_origin);
    OperationFinishedInfo(const OperationFinishedInfo&);
    OperationFinishedInfo(OperationFinishedInfo&&);
    ~OperationFinishedInfo();
    OperationFinishedInfo& operator=(const OperationFinishedInfo&);
    OperationFinishedInfo& operator=(OperationFinishedInfo&&);
  };

  TestSharedStorageObserver();
  ~TestSharedStorageObserver() override;

  GlobalRenderFrameHostId AssociatedFrameHostId() const override;

  bool ShouldReceiveAllSharedStorageReports() const override;

  void OnSharedStorageAccessed(base::Time access_time,
                               AccessScope scope,
                               AccessMethod method,
                               GlobalRenderFrameHostId main_frame_id,
                               const std::string& owner_origin,
                               const SharedStorageEventParams& params) override;

  void OnSharedStorageSelectUrlUrnUuidGenerated(const GURL& urn_uuid) override;

  void OnSharedStorageSelectUrlConfigPopulated(
      const std::optional<FencedFrameConfig>& config) override;

  void OnSharedStorageWorkletOperationExecutionFinished(
      base::Time finished_time,
      base::TimeDelta execution_time,
      AccessMethod method,
      int operation_id,
      const base::UnguessableToken& worklet_devtools_token,
      GlobalRenderFrameHostId main_frame_id,
      const std::string& owner_origin) override;

  void ExpectAccessObserved(const std::vector<Access>& expected_accesses);

  void ExpectOperationFinishedInfosObserved(
      const std::vector<OperationFinishedInfo>& expected_infos);

  const std::vector<GURL>& urn_uuids_observed() const {
    return urn_uuids_observed_;
  }

 private:
  std::vector<Access> accesses_;
  std::vector<GURL> urn_uuids_observed_;
  std::vector<OperationFinishedInfo> operation_finished_infos_;
};

std::ostream& operator<<(std::ostream& os,
                         const TestSharedStorageObserver::Access& access);

bool operator==(const TestSharedStorageObserver::OperationFinishedInfo& lhs,
                const TestSharedStorageObserver::OperationFinishedInfo& rhs);

std::ostream& operator<<(
    std::ostream& os,
    const TestSharedStorageObserver::OperationFinishedInfo& info);

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_OBSERVER_H_
