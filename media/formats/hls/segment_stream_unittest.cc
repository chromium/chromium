// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/segment_stream.h"

#include <optional>

#include "base/logging.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/test_helpers.h"
#include "media/formats/hls/media_playlist_test_builder.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

using testing::_;

namespace {

template <typename... Strings>
scoped_refptr<MediaPlaylist> CreateMediaPlaylist(Strings... strings) {
  MediaPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  ([&] { builder.AppendLine(strings); }(), ...);
  return builder.Parse<const MultivariantPlaylist*>(nullptr);
}

}  // namespace

TEST(SegmentStreamUnittest, BasicQueueUsage) {
  auto segment_stream = std::make_unique<SegmentStream>(
      CreateMediaPlaylist("#EXT-X-TARGETDURATION:10", "#EXT-X-VERSION:1",
                          "#EXT-X-MEDIA-SEQUENCE:0",
                          "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD", "#EXTINF:9.2,",
                          "video.ts", "#EXT-X-ENDLIST"),
      /*seekable=*/true);

  ASSERT_TRUE(segment_stream->PlaylistHasSegments());
  ASSERT_EQ(segment_stream->GetMaxDuration(), base::Seconds(10));
  ASSERT_FALSE(segment_stream->Exhausted());
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(0));

  scoped_refptr<hls::MediaSegment> segment;
  base::TimeDelta start;
  base::TimeDelta end;
  std::tie(segment, start, end) = segment_stream->GetNextSegment();

  ASSERT_TRUE(segment_stream->PlaylistHasSegments());
  ASSERT_EQ(segment_stream->GetMaxDuration(), base::Seconds(10));
  ASSERT_TRUE(segment_stream->Exhausted());
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));

  ASSERT_EQ(segment->GetUri().path(), "/video.ts");
}

TEST(SegmentStreamUnittest, SeekInQueue) {
  auto segment_stream = std::make_unique<SegmentStream>(
      CreateMediaPlaylist("#EXT-X-TARGETDURATION:10", "#EXT-X-VERSION:1",
                          "#EXT-X-MEDIA-SEQUENCE:0",
                          "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD", "#EXTINF:9.2,",
                          "video1.ts", "#EXTINF:9.2,", "video2.ts",
                          "#EXT-X-ENDLIST"),
      /*seekable=*/true);

  ASSERT_TRUE(segment_stream->PlaylistHasSegments());
  ASSERT_EQ(segment_stream->GetMaxDuration(), base::Seconds(10));
  ASSERT_FALSE(segment_stream->Exhausted());
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(0));

  // Pop the first one out.
  segment_stream->GetNextSegment();
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));

  // Seek to the beginning
  ASSERT_TRUE(segment_stream->Seek(base::Seconds(0)));
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(0));

  // Seek to the second segment.
  ASSERT_TRUE(segment_stream->Seek(base::Seconds(10)));
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));

  // just more random seeks.
  ASSERT_TRUE(segment_stream->Seek(base::Seconds(13)));
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));

  ASSERT_TRUE(segment_stream->Seek(base::Seconds(5)));
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(0));

  ASSERT_TRUE(segment_stream->Seek(base::Seconds(14)));
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));

  // Seek outside the total time. Should return false and not update the segment
  // queue.
  ASSERT_FALSE(segment_stream->Seek(base::Seconds(222)));
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));
}

TEST(SegmentStreamUnittest, SeekDisallowed) {
  auto segment_stream = std::make_unique<SegmentStream>(
      CreateMediaPlaylist("#EXT-X-TARGETDURATION:10", "#EXT-X-VERSION:1",
                          "#EXT-X-MEDIA-SEQUENCE:0",
                          "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD", "#EXTINF:9.2,",
                          "video1.ts", "#EXTINF:9.2,", "video2.ts",
                          "#EXT-X-ENDLIST"),
      /*seekable=*/false);

  ASSERT_TRUE(segment_stream->PlaylistHasSegments());
  ASSERT_EQ(segment_stream->GetMaxDuration(), base::Seconds(10));
  ASSERT_FALSE(segment_stream->Exhausted());
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(0));

  // Pop the first one out.
  segment_stream->GetNextSegment();
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));

  // Seek to the beginning. not allowed.
  ASSERT_FALSE(segment_stream->Seek(base::Seconds(0)));
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));
}

TEST(SegmentStreamUnittest, SeekablePlaylistAdapt) {
  auto segment_stream = std::make_unique<SegmentStream>(
      CreateMediaPlaylist(
          "#EXT-X-TARGETDURATION:10", "#EXT-X-VERSION:1",
          "#EXT-X-MEDIA-SEQUENCE:0", "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD",
          "#EXTINF:9.2,", "video1_low.ts", "#EXTINF:9.2,", "video2_low.ts",
          "#EXTINF:9.2,", "video3_low.ts", "#EXTINF:9.2,", "video4_low.ts",
          "#EXTINF:9.2,", "video5_low.ts", "#EXT-X-ENDLIST"),
      /*seekable=*/true);

  ASSERT_TRUE(segment_stream->PlaylistHasSegments());
  ASSERT_EQ(segment_stream->GetMaxDuration(), base::Seconds(10));
  ASSERT_FALSE(segment_stream->Exhausted());
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(0));

  // Read the first element
  scoped_refptr<hls::MediaSegment> segment;
  base::TimeDelta start;
  base::TimeDelta end;
  std::tie(segment, start, end) = segment_stream->GetNextSegment();

  // Check the segment is low quality.
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));
  ASSERT_EQ(segment->GetUri().path(), "/video1_low.ts");

  // Quality scaled up, a new playlist is injected:
  segment_stream->SetNewPlaylist(CreateMediaPlaylist(
      "#EXT-X-TARGETDURATION:10", "#EXT-X-VERSION:1", "#EXT-X-MEDIA-SEQUENCE:0",
      "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD", "#EXTINF:9.2,", "video1_high.ts",
      "#EXTINF:9.2,", "video2_high.ts", "#EXTINF:9.2,", "video3_high.ts",
      "#EXTINF:9.2,", "video4_high.ts", "#EXTINF:9.2,", "video5_high.ts",
      "#EXT-X-ENDLIST"));

  // The next segment should now be the video2_high file.
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(9.2));
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(18.4));
  ASSERT_EQ(segment->GetUri().path(), "/video2_high.ts");

  // Seek to the last segment:
  ASSERT_TRUE(segment_stream->Seek(base::Seconds(45)));
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(36.8));

  // pop it.
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video5_high.ts");
  ASSERT_TRUE(segment_stream->Exhausted());

  // adapt downwards:
  segment_stream->SetNewPlaylist(CreateMediaPlaylist(
      "#EXT-X-TARGETDURATION:10", "#EXT-X-VERSION:1", "#EXT-X-MEDIA-SEQUENCE:0",
      "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD", "#EXTINF:9.2,", "video1_low.ts",
      "#EXTINF:9.2,", "video2_low.ts", "#EXTINF:9.2,", "video3_low.ts",
      "#EXTINF:9.2,", "video4_low.ts", "#EXTINF:9.2,", "video5_low.ts",
      "#EXT-X-ENDLIST"));

  // The stream is still exhausted.
  ASSERT_TRUE(segment_stream->Exhausted());

  // Seek backwards and read from the stream, should be low video.
  ASSERT_TRUE(segment_stream->Seek(base::Seconds(20)));
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video3_low.ts");
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video4_low.ts");
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video5_low.ts");
}

TEST(SegmentStreamUnittest, RealWorldExample) {
  auto segment_stream = std::make_unique<SegmentStream>(
      CreateMediaPlaylist("#EXT-X-VERSION:1", "#EXT-X-TARGETDURATION:2",
                          "#EXT-X-MEDIA-SEQUENCE:20909320", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909320.ts", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909321.ts", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909322.ts", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909323.ts", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909324.ts", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909325.ts", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909326.ts", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909327.ts", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909328.ts", "#EXTINF:2.00000,",
                          "playlist_800Kb_20909329.ts"),
      false);

  ASSERT_TRUE(segment_stream->PlaylistHasSegments());
  ASSERT_EQ(segment_stream->GetMaxDuration(), base::Seconds(2));
  ASSERT_FALSE(segment_stream->Exhausted());
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(0));

  scoped_refptr<hls::MediaSegment> segment;
  base::TimeDelta start;
  base::TimeDelta end;

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/playlist_800Kb_20909320.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 20909320lu);

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/playlist_800Kb_20909321.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 20909321lu);

  segment_stream->SetNewPlaylist(
      CreateMediaPlaylist("#EXT-X-VERSION:1", "#EXT-X-TARGETDURATION:2",
                          "#EXT-X-MEDIA-SEQUENCE:20909321", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909321.ts", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909322.ts", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909323.ts", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909324.ts", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909325.ts", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909326.ts", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909327.ts", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909328.ts", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909329.ts", "#EXTINF:2.00000,",
                          "playlist_2500Kb_20909330.ts"));

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/playlist_2500Kb_20909322.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 20909322lu);

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/playlist_2500Kb_20909323.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 20909323lu);
}

TEST(SegmentStreamUnittest, UnseekablePlaylistAdapt) {
  auto segment_stream = std::make_unique<SegmentStream>(
      CreateMediaPlaylist("#EXT-X-TARGETDURATION:10", "#EXT-X-VERSION:1",
                          "#EXT-X-MEDIA-SEQUENCE:10",
                          "#EXT-X-MEDIA-PLAYLIST-TYPE:LIVE", "#EXTINF:9.2,",
                          "video10_low.ts", "#EXTINF:9.2,", "video11_low.ts",
                          "#EXTINF:9.2,", "video12_low.ts", "#EXTINF:9.2,",
                          "video13_low.ts", "#EXTINF:9.2,", "video14_low.ts"),
      /*seekable=*/false);

  ASSERT_TRUE(segment_stream->PlaylistHasSegments());
  ASSERT_EQ(segment_stream->GetMaxDuration(), base::Seconds(10));
  ASSERT_FALSE(segment_stream->Exhausted());
  ASSERT_EQ(segment_stream->NextSegmentStartTime(), base::Seconds(0));

  scoped_refptr<hls::MediaSegment> segment;
  base::TimeDelta start;
  base::TimeDelta end;

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video10_low.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 10lu);
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video11_low.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 11lu);
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video12_low.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 12lu);
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video13_low.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 13lu);
  ASSERT_FALSE(segment_stream->Exhausted());

  // The stream just has 14 left. For "unseekable" aka live playlists, we not
  // only update when there is an adaptation, but also when there is an update
  // to the live stream and there are new segments ready. Often these sets of
  // segments overlap. So lets append an updated, same-quality playlist. This
  // should not re-add the 12th or 13th segments. When we pop again, we should
  // pick right up at 14.
  segment_stream->SetNewPlaylist(CreateMediaPlaylist(
      "#EXT-X-TARGETDURATION:10", "#EXT-X-VERSION:1",
      "#EXT-X-MEDIA-SEQUENCE:12", "#EXT-X-MEDIA-PLAYLIST-TYPE:LIVE",
      "#EXTINF:9.2,", "video12_low.ts", "#EXTINF:9.2,", "video13_low.ts",
      "#EXTINF:9.2,", "video14_low.ts", "#EXTINF:9.2,", "video15_low.ts",
      "#EXTINF:9.2,", "video16_low.ts", "#EXT-X-ENDLIST"));

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video14_low.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 14lu);

  // The original stream had only up to segment 14. But since we've added new
  // content, we should now still have more left.
  ASSERT_FALSE(segment_stream->Exhausted());

  // Read the rest out
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video15_low.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 15lu);
  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video16_low.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 16lu);

  ASSERT_TRUE(segment_stream->Exhausted());

  // Appending more segments now should still not add anything that came
  // before the last segment that was added.
  segment_stream->SetNewPlaylist(CreateMediaPlaylist(
      "#EXT-X-TARGETDURATION:10", "#EXT-X-VERSION:1",
      "#EXT-X-MEDIA-SEQUENCE:15", "#EXT-X-MEDIA-PLAYLIST-TYPE:LIVE",
      "#EXTINF:9.2,", "video15_low.ts", "#EXTINF:9.2,", "video16_low.ts",
      "#EXTINF:9.2,", "video17_low.ts", "#EXTINF:9.2,", "video18_low.ts",
      "#EXTINF:9.2,", "video19_low.ts", "#EXT-X-ENDLIST"));
  ASSERT_FALSE(segment_stream->Exhausted());

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/video17_low.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 17lu);
  ASSERT_FALSE(segment_stream->Exhausted());
}

TEST(SegmentStreamUnittest, SeekableEncryptedPlaylistAdapt) {
  auto segment_stream = std::make_unique<SegmentStream>(
      CreateMediaPlaylist(
          "#EXT-X-TARGETDURATION:12", "#EXT-X-VERSION:1",
          "#EXT-X-MEDIA-SEQUENCE:0", "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD",
          "#EXTINF:12.00000,",       // Encrypted content commonly leads with
          "playlist_100000Kb_0.ts",  // a few clear segments.
          "#EXT-X-KEY:METHOD=AES-128,URI=\"example.com/key1\"",
          "#EXTINF:12.0000000,", "playlist_100000Kb_1.ts",
          "#EXTINF:12.0000000,", "playlist_100000Kb_2.ts",
          "#EXTINF:12.0000000,", "playlist_100000Kb_3.ts",
          "#EXTINF:12.0000000,", "playlist_100000Kb_4.ts",
          "#EXT-X-KEY:METHOD=AES-128,URI=\"example.com/key2\"",
          "#EXTINF:12.0000000,", "playlist_100000Kb_5.ts",
          "#EXTINF:12.0000000,", "playlist_100000Kb_6.ts",
          "#EXTINF:12.0000000,", "playlist_100000Kb_7.ts",
          "#EXTINF:12.0000000,", "playlist_100000Kb_8.ts"),
      true);

  scoped_refptr<hls::MediaSegment> segment;
  base::TimeDelta start;
  base::TimeDelta end;

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/playlist_100000Kb_0.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 0lu);

  // If we load a new playlist now, it will replace segment 1, because despite
  // being encrypted, it has a new enc data structure.
  segment_stream->SetNewPlaylist(CreateMediaPlaylist(
      "#EXT-X-TARGETDURATION:12", "#EXT-X-VERSION:1", "#EXT-X-MEDIA-SEQUENCE:0",
      "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD",
      "#EXTINF:12.00000,",       // Encrypted content commonly leads with
      "playlist_200000Kb_0.ts",  // a few clear segments.
      "#EXT-X-KEY:METHOD=AES-128,URI=\"example.com/key3\"",
      "#EXTINF:12.0000000,", "playlist_200000Kb_1.ts", "#EXTINF:12.0000000,",
      "playlist_200000Kb_2.ts", "#EXTINF:12.0000000,", "playlist_200000Kb_3.ts",
      "#EXTINF:12.0000000,", "playlist_200000Kb_4.ts",
      "#EXT-X-KEY:METHOD=AES-128,URI=\"example.com/key4\"",
      "#EXTINF:12.0000000,", "playlist_200000Kb_5.ts", "#EXTINF:12.0000000,",
      "playlist_200000Kb_6.ts", "#EXTINF:12.0000000,", "playlist_200000Kb_7.ts",
      "#EXTINF:12.0000000,", "playlist_200000Kb_8.ts"));

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/playlist_200000Kb_1.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 1lu);

  // Now though, we'll keep the next segment, because it's encrypted with an
  // existing key. The segment after that will be replaced however.
  segment_stream->SetNewPlaylist(CreateMediaPlaylist(
      "#EXT-X-TARGETDURATION:12", "#EXT-X-VERSION:1", "#EXT-X-MEDIA-SEQUENCE:0",
      "#EXT-X-MEDIA-PLAYLIST-TYPE:VOD",
      "#EXTINF:12.00000,",       // Encrypted content commonly leads with
      "playlist_300000Kb_0.ts",  // a few clear segments.
      "#EXT-X-KEY:METHOD=AES-128,URI=\"example.com/key3\"",
      "#EXTINF:12.0000000,", "playlist_300000Kb_1.ts", "#EXTINF:12.0000000,",
      "playlist_300000Kb_2.ts", "#EXTINF:12.0000000,", "playlist_300000Kb_3.ts",
      "#EXTINF:12.0000000,", "playlist_300000Kb_4.ts",
      "#EXT-X-KEY:METHOD=AES-128,URI=\"example.com/key4\"",
      "#EXTINF:12.0000000,", "playlist_300000Kb_5.ts", "#EXTINF:12.0000000,",
      "playlist_300000Kb_6.ts", "#EXTINF:12.0000000,", "playlist_300000Kb_7.ts",
      "#EXTINF:12.0000000,", "playlist_300000Kb_8.ts"));

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/playlist_200000Kb_2.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 2lu);

  std::tie(segment, start, end) = segment_stream->GetNextSegment();
  ASSERT_EQ(segment->GetUri().path(), "/playlist_300000Kb_3.ts");
  ASSERT_EQ(segment->GetMediaSequenceNumber(), 3lu);
}

}  // namespace media::hls
