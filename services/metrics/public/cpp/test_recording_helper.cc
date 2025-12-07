// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/test_recording_helper.h"

namespace ukm {

TestRecordingHelper::TestRecordingHelper(UkmRecorder* recorder)
    : recorder_(recorder) {
  recorder_->SetSamplingForTesting(1);
}

void TestRecordingHelper::UpdateSourceURL(SourceId source_id, const GURL& url) {
  recorder_->UpdateSourceURL(source_id, url);
}

void TestRecordingHelper::RecordNavigation(
    SourceId source_id,
    const UkmSource::NavigationData& navigation_data) {
  recorder_->RecordNavigation(source_id, navigation_data);
}

void TestRecordingHelper::MarkSourceForDeletion(SourceId source_id) {
  recorder_->MarkSourceForDeletion(source_id);
}

void TestRecordingHelper::RecordWebDXFeatures(SourceId source_id,
                                              const std::set<int32_t>& features,
                                              const size_t max_feature_value) {
  recorder_->RecordWebDXFeatures(source_id, features, max_feature_value);
}

}  // namespace ukm
