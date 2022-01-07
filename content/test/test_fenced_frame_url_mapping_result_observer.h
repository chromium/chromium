// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_FENCED_FRAME_URL_MAPPING_RESULT_OBSERVER_H_
#define CONTENT_TEST_TEST_FENCED_FRAME_URL_MAPPING_RESULT_OBSERVER_H_

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

namespace content {

// Tests can use this class to observe and check the URL mapping result.
class TestFencedFrameURLMappingResultObserver
    : public FencedFrameURLMapping::MappingResultObserver {
 public:
  TestFencedFrameURLMappingResultObserver();
  ~TestFencedFrameURLMappingResultObserver() override;

  void OnFencedFrameURLMappingComplete(
      absl::optional<GURL> mapped_url,
      absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
          pending_ad_components_map) override;

  bool mapping_complete_observed() const { return mapping_complete_observed_; }

  const absl::optional<GURL>& mapped_url() const { return mapped_url_; }

  const absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>&
  pending_ad_components_map() const {
    return pending_ad_components_map_;
  }

 private:
  bool mapping_complete_observed_ = false;
  absl::optional<GURL> mapped_url_;
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
      pending_ad_components_map_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_FENCED_FRAME_URL_MAPPING_RESULT_OBSERVER_H_
