// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_fenced_frame_url_mapping_result_observer.h"

namespace content {

TestFencedFrameURLMappingResultObserver::
    TestFencedFrameURLMappingResultObserver() = default;

TestFencedFrameURLMappingResultObserver::
    ~TestFencedFrameURLMappingResultObserver() = default;

void TestFencedFrameURLMappingResultObserver::OnFencedFrameURLMappingComplete(
    absl::optional<GURL> mapped_url,
    absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
        pending_ad_components_map) {
  mapping_complete_observed_ = true;
  mapped_url_ = std::move(mapped_url);
  pending_ad_components_map_ = std::move(pending_ad_components_map);
}

}  // namespace content
