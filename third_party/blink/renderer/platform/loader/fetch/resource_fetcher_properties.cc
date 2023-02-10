// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

#include "third_party/blink/renderer/platform/heap/visitor.h"
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
  is_outermost_main_frame_ = properties_->IsOutermostMainFrame();
  paused_ = properties_->IsPaused();
  freeze_mode_ = properties_->FreezeMode();
  load_complete_ = properties_->IsLoadComplete();
  is_subframe_deprioritization_enabled_ =
      properties_->IsSubframeDeprioritizationEnabled();
  outstanding_throttled_limit_ = properties_->GetOutstandingThrottledLimit();

  properties_ = nullptr;
}

void DetachableResourceFetcherProperties::Trace(Visitor* visitor) const {
  visitor->Trace(properties_);
  visitor->Trace(fetch_client_settings_object_);
  ResourceFetcherProperties::Trace(visitor);
}

}  // namespace blink
