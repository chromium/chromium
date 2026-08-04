// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/html_media_track_element_media_track.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_camera_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HTMLMediaTrackElementMediaTrackTest : public PageTestBase {};

TEST_F(HTMLMediaTrackElementMediaTrackTest, TrackInitializationAndRetrieval) {
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* element = MakeGarbageCollected<HTMLCameraElement>(GetDocument());

  // Should lazily initialize and return nullptr track initially
  MediaStreamTrack* track = HTMLMediaTrackElementMediaTrack::track(*element);
  EXPECT_EQ(track, nullptr);
}

}  // namespace blink

