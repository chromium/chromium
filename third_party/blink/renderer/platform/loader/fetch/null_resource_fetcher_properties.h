// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_NULL_RESOURCE_FETCHER_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_NULL_RESOURCE_FETCHER_PROPERTIES_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// NullResourceFetcherProperties is a ResourceFetcherProperties implementation
// which returns default values.
// This is used for ResourceFetchers with a detached document, as well as tests.
class PLATFORM_EXPORT NullResourceFetcherProperties final
    : public ResourceFetcherProperties {
 public:
  NullResourceFetcherProperties();
  ~NullResourceFetcherProperties() override = default;

  void Trace(Visitor*) override;

  // ResourceFetcherProperties implementation
  const FetchClientSettingsObject& GetFetchClientSettingsObject()
      const override {
    return *fetch_client_settings_object_;
  }
  bool IsMainFrame() const override { return false; }
  ControllerServiceWorkerMode GetControllerServiceWorkerMode() const override {
    return ControllerServiceWorkerMode::kNoController;
  }
  int64_t ServiceWorkerId() const override {
    NOTREACHED();
    return 0;
  }
  bool IsPaused() const override { return false; }
  bool IsDetached() const override { return true; }
  bool IsLoadComplete() const override { return true; }
  bool ShouldBlockLoadingSubResource() const override { return true; }
  bool IsSubframeDeprioritizationEnabled() const override { return false; }
  scheduler::FrameStatus GetFrameStatus() const override {
    return scheduler::FrameStatus::kNone;
  }

 private:
  const Member<const FetchClientSettingsObject> fetch_client_settings_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_NULL_RESOURCE_FETCHER_PROPERTIES_H_
