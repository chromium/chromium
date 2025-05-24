// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/text_track_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(TextTrackListTest, InvalidateTrackIndexes) {
  test::TaskEnvironment task_environment;
  // Create and fill the list
  auto* list = MakeGarbageCollected<TextTrackList>(
      MakeGarbageCollected<HTMLVideoElement>(
          std::make_unique<DummyPageHolder>()->GetDocument()));
  const size_t kNumTextTracks = 4;
  std::array<TextTrack*, kNumTextTracks> text_tracks;
  for (size_t i = 0; i < kNumTextTracks; ++i) {
    text_tracks[i] = MakeGarbageCollected<TextTrack>(
        V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles), g_empty_atom,
        g_empty_atom, *list->Owner());
    list->Append(text_tracks[i]);
  }

  EXPECT_EQ(4u, list->length());
  EXPECT_EQ(0, text_tracks[0]->TrackIndex());
  EXPECT_EQ(1, text_tracks[1]->TrackIndex());
  EXPECT_EQ(2, text_tracks[2]->TrackIndex());
  EXPECT_EQ(3, text_tracks[3]->TrackIndex());

  // Remove element from the middle of the list
  list->Remove(text_tracks[1]);

  EXPECT_EQ(3u, list->length());
  EXPECT_EQ(nullptr, text_tracks[1]->TrackList());
  EXPECT_EQ(0, text_tracks[0]->TrackIndex());
  EXPECT_EQ(1, text_tracks[2]->TrackIndex());
  EXPECT_EQ(2, text_tracks[3]->TrackIndex());
}

}  // namespace blink
