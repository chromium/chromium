// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

namespace blink {

void DetachableResourceFetcherProperties::Detach() {
  if (!properties_) {
    // Already detached.
    return;
  }

  fetch_client_settings_object_ =
      MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          properties_->GetFetchClientSettingsObject());
  is_main_frame_ = properties_->IsMainFrame();
  paused_ = properties_->IsPaused();
  load_complete_ = properties_->IsLoadComplete();
  is_subframe_deprioritization_enabled_ =
      properties_->IsSubframeDeprioritizationEnabled();

  properties_ = nullptr;
}

void DetachableResourceFetcherProperties::Trace(Visitor* visitor) {
  visitor->Trace(properties_);
  visitor->Trace(fetch_client_settings_object_);
  ResourceFetcherProperties::Trace(visitor);
}

}  // namespace blink
