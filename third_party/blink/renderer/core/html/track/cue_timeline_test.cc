// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/cue_timeline.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

// Regression test: CueEventTimerFired should not crash when poster flag is set.
TEST(CueTimelineTest, CueEventTimerFiredWithPosterFlagSet) {
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>();
  auto* video =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());

  // Video starts with show_poster_flag_ = true
  ASSERT_TRUE(video->IsShowPosterFlagSet());

  // Create text track to initialize CueTimeline
  TextTrack* track = MakeGarbageCollected<TextTrack>(
      V8TextTrackKind(V8TextTrackKind::Enum::kCaptions), g_empty_atom,
      g_empty_atom, *video);
  video->textTracks()->Append(track);

  CueTimeline& cue_timeline = video->GetCueTimeline();

  // Should not crash - guard in CueEventTimerFired
  cue_timeline.CueEventTimerFired(nullptr);
}

}  // namespace blink
