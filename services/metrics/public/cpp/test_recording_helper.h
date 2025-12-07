// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_TEST_RECORDING_HELPER_H_
#define SERVICES_METRICS_PUBLIC_CPP_TEST_RECORDING_HELPER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "url/gurl.h"

namespace ukm {

// Used to give unit tests access to private functions in UkmRecorder. This is
// only available for testing.
class TestRecordingHelper {
 public:
  explicit TestRecordingHelper(UkmRecorder* recorder);

  TestRecordingHelper(const TestRecordingHelper&) = delete;
  TestRecordingHelper& operator=(const TestRecordingHelper&) = delete;

  void UpdateSourceURL(SourceId source_id, const GURL& url);

  void RecordNavigation(SourceId source_id,
                        const UkmSource::NavigationData& navigation_data);

  void MarkSourceForDeletion(SourceId source_id);

  void RecordWebDXFeatures(SourceId source_id,
                           const std::set<int32_t>& features,
                           size_t max_feature_value);

 private:
  raw_ptr<UkmRecorder> recorder_;
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_TEST_RECORDING_HELPER_H_
