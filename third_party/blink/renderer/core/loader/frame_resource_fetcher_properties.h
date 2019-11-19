// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_RESOURCE_FETCHER_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_RESOURCE_FETCHER_PROPERTIES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

namespace blink {

class FrameOrImportedDocument;

// FrameResourceFetcherProperties is a ResourceFetcherProperties implementation
// for Frame.
class CORE_EXPORT FrameResourceFetcherProperties final
    : public ResourceFetcherProperties {
 public:
  explicit FrameResourceFetcherProperties(FrameOrImportedDocument&);
  ~FrameResourceFetcherProperties() override = default;

  void Trace(Visitor*) override;

  const FrameOrImportedDocument& GetFrameOrImportedDocument() const {
    return *frame_or_imported_document_;
  }

  // ResourceFetcherProperties implementation
  const FetchClientSettingsObject& GetFetchClientSettingsObject()
      const override {
    return *fetch_client_settings_object_;
  }
  bool IsMainFrame() const override;
  ControllerServiceWorkerMode GetControllerServiceWorkerMode() const override;
  int64_t ServiceWorkerId() const override;
  bool IsPaused() const override;
  bool IsDetached() const override { return false; }
  bool IsLoadComplete() const override;
  bool ShouldBlockLoadingSubResource() const override;
  bool IsSubframeDeprioritizationEnabled() const override;
  scheduler::FrameStatus GetFrameStatus() const override;

 private:
  const Member<FrameOrImportedDocument> frame_or_imported_document_;
  Member<const FetchClientSettingsObject> fetch_client_settings_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_RESOURCE_FETCHER_PROPERTIES_H_
