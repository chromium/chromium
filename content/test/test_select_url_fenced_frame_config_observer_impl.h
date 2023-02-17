// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_SELECT_URL_FENCED_FRAME_CONFIG_OBSERVER_IMPL_H_
#define CONTENT_TEST_TEST_SELECT_URL_FENCED_FRAME_CONFIG_OBSERVER_IMPL_H_

#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"

class GURL;

namespace content {

class TestSelectURLFencedFrameConfigObserverImpl
    : public SharedStorageWorkletHostManager::SharedStorageObserverInterface {
 public:
  TestSelectURLFencedFrameConfigObserverImpl();
  ~TestSelectURLFencedFrameConfigObserverImpl() override;

  void OnSharedStorageAccessed(const base::Time& access_time,
                               AccessType type,
                               const std::string& main_frame_id,
                               const std::string& owner_origin,
                               const SharedStorageEventParams& params) override;
  void OnUrnUuidGenerated(const GURL& urn_uuid) override;
  void OnConfigPopulated(
      const absl::optional<FencedFrameConfig>& config) override;

  const absl::optional<GURL>& GetUrnUuid() const;
  const absl::optional<FencedFrameConfig>& GetConfig() const;
  bool ConfigObserved() const;

 private:
  absl::optional<GURL> urn_uuid_;
  absl::optional<FencedFrameConfig> config_;
  bool config_observed_ = false;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_SELECT_URL_FENCED_FRAME_CONFIG_OBSERVER_IMPL_H_