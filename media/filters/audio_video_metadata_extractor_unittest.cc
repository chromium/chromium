// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_video_metadata_extractor.h"

#include <memory>

#include "base/hash/sha1.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "media/base/test_data_util.h"
#include "media/filters/file_data_source.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

std::unique_ptr<AudioVideoMetadataExtractor> GetExtractor(
    const std::string& filename,
    bool extract_attached_images,
    bool expected_result,
    double expected_duration,
    int expected_width,
    int expected_height) {
  FileDataSource source;
  EXPECT_TRUE(source.Initialize(GetTestDataFilePath(filename)));

  std::unique_ptr<AudioVideoMetadataExtractor> extractor(
      new AudioVideoMetadataExtractor);
  bool extracted = extractor->Extract(&source, extract_attached_images);
  EXPECT_EQ(expected_result, extracted);

  if (!extracted)
    return extractor;

  EXPECT_TRUE(extractor->has_duration());
  EXPECT_EQ(expected_duration, extractor->duration());

  EXPECT_EQ(expected_width, extractor->width());
  EXPECT_EQ(expected_height, extractor->height());

  return extractor;
}

const std::string GetTagValue(
    const media::AudioVideoMetadataExtractor::TagDictionary& tags,
    const char* tag_name) {
  auto tag_data = tags.find(tag_name);
  if (tag_data == tags.end()) {
    DLOG(WARNING) << "Tag name \"" << tag_name << "\" not found!";
    return "";
  }

  return tag_data->second;
}

TEST(AudioVideoMetadataExtractorTest, InvalidFile) {
  GetExtractor("ten_byte_file", true, false, 0, -1, -1);
}

TEST(AudioVideoMetadataExtractorTest, AudioOGG) {
  std::unique_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("9ch.ogg", true, true, 0.1, -1, -1);
  EXPECT_EQ("Processed by SoX", extractor->comment());

  EXPECT_EQ("ogg", extractor->stream_infos()[0].type);
  EXPECT_EQ(2u, extractor->stream_infos().size());

  EXPECT_EQ(0u, extractor->stream_infos()[0].tags.size());

  EXPECT_EQ(1u, extractor->stream_infos()[1].tags.size());
  EXPECT_EQ("vorbis", extractor->stream_infos()[1].type);
  EXPECT_EQ("Processed by SoX",
            GetTagValue(extractor->stream_infos()[1].tags, "Comment"));

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}

TEST(AudioVideoMetadataExtractorTest, AudioWAV) {
  std::unique_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("sfx_u8.wav", true, true, 0.288413, -1, -1);
  EXPECT_EQ("Lavf54.37.100", extractor->encoder());
  EXPECT_EQ("Amadeus Pro", extractor->encoded_by());

  EXPECT_EQ("wav", extractor->stream_infos()[0].type);
  EXPECT_EQ(2u, extractor->stream_infos().size());

  EXPECT_EQ(2u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ("Lavf54.37.100",
            GetTagValue(extractor->stream_infos()[0].tags, "encoder"));
  EXPECT_EQ("Amadeus Pro",
            GetTagValue(extractor->stream_infos()[0].tags, "encoded_by"));

  EXPECT_EQ("pcm_u8", extractor->stream_infos()[1].type);
  EXPECT_EQ(0u, extractor->stream_infos()[1].tags.size());

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}

TEST(AudioVideoMetadataExtractorTest, AudioFLAC) {
  std::unique_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("sfx.flac", true, true, 0.288413, -1, -1);
  EXPECT_EQ("Lavf55.43.100", extractor->encoder());
  EXPECT_EQ("Amadeus Pro", extractor->encoded_by());

  EXPECT_EQ("flac", extractor->stream_infos()[0].type);
  EXPECT_EQ(2u, extractor->stream_infos().size());

  EXPECT_EQ(2u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ("Lavf55.43.100",
            GetTagValue(extractor->stream_infos()[0].tags, "encoder"));
  EXPECT_EQ("Amadeus Pro",
            GetTagValue(extractor->stream_infos()[0].tags, "encoded_by"));

  EXPECT_EQ("flac", extractor->stream_infos()[1].type);
  EXPECT_EQ(0u, extractor->stream_infos()[1].tags.size());

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}

TEST(AudioVideoMetadataExtractorTest, VideoWebM) {
  std::unique_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("bear-320x240-multitrack.webm", true, true, 2.744, 320, 240);
  EXPECT_EQ("Lavf53.9.0", extractor->encoder());

  EXPECT_EQ(6u, extractor->stream_infos().size());

  EXPECT_EQ("matroska,webm", extractor->stream_infos()[0].type);
  EXPECT_EQ(1u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ("Lavf53.9.0",
            GetTagValue(extractor->stream_infos()[0].tags, "ENCODER"));

  EXPECT_EQ("vp8", extractor->stream_infos()[1].type);
  EXPECT_EQ(0u, extractor->stream_infos()[1].tags.size());

  EXPECT_EQ("vorbis", extractor->stream_infos()[2].type);
  EXPECT_EQ(0u, extractor->stream_infos()[2].tags.size());

  EXPECT_EQ("subrip", extractor->stream_infos()[3].type);
  EXPECT_EQ(0u, extractor->stream_infos()[3].tags.size());

  EXPECT_EQ("theora", extractor->stream_infos()[4].type);
  EXPECT_EQ(0u, extractor->stream_infos()[4].tags.size());

  EXPECT_EQ("pcm_s16le", extractor->stream_infos()[5].type);
  EXPECT_EQ(1u, extractor->stream_infos()[5].tags.size());
  EXPECT_EQ("Lavc52.32.0",
            GetTagValue(extractor->stream_infos()[5].tags, "ENCODER"));

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST(AudioVideoMetadataExtractorTest, AndroidRotatedMP4Video) {
  std::unique_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("90rotation.mp4", true, true, 0.196, 1920, 1080);

  EXPECT_EQ(90, extractor->rotation());

  EXPECT_EQ(3u, extractor->stream_infos().size());

  EXPECT_EQ("mov,mp4,m4a,3gp,3g2,mj2", extractor->stream_infos()[0].type);
  EXPECT_EQ(4u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ("isom3gp4", GetTagValue(extractor->stream_infos()[0].tags,
                                    "compatible_brands"));
  EXPECT_EQ("2014-02-11T00:39:25.000000Z",
            GetTagValue(extractor->stream_infos()[0].tags, "creation_time"));
  EXPECT_EQ("isom",
            GetTagValue(extractor->stream_infos()[0].tags, "major_brand"));
  EXPECT_EQ("0",
            GetTagValue(extractor->stream_infos()[0].tags, "minor_version"));

  EXPECT_EQ("h264", extractor->stream_infos()[1].type);
  EXPECT_EQ(5u, extractor->stream_infos()[1].tags.size());
  EXPECT_EQ("2014-02-11T00:39:25.000000Z",
            GetTagValue(extractor->stream_infos()[1].tags, "creation_time"));
  EXPECT_EQ("VideoHandle",
            GetTagValue(extractor->stream_infos()[1].tags, "handler_name"));
  EXPECT_EQ("eng", GetTagValue(extractor->stream_infos()[1].tags, "language"));
  EXPECT_EQ("90", GetTagValue(extractor->stream_infos()[1].tags, "rotate"));

  EXPECT_EQ("aac", extractor->stream_infos()[2].type);
  EXPECT_EQ(3u, extractor->stream_infos()[2].tags.size());
  EXPECT_EQ("2014-02-11T00:39:25.000000Z",
            GetTagValue(extractor->stream_infos()[2].tags, "creation_time"));
  EXPECT_EQ("SoundHandle",
            GetTagValue(extractor->stream_infos()[2].tags, "handler_name"));
  EXPECT_EQ("eng", GetTagValue(extractor->stream_infos()[2].tags, "language"));

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

TEST(AudioVideoMetadataExtractorTest, AudioMP3) {
  std::unique_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("id3_png_test.mp3", true, true, 1.018776, -1, -1);

  EXPECT_EQ("Airbag", extractor->title());
  EXPECT_EQ("Radiohead", extractor->artist());
  EXPECT_EQ("OK Computer", extractor->album());
  EXPECT_EQ(1, extractor->track());
  EXPECT_EQ("Alternative", extractor->genre());
  EXPECT_EQ("1997", extractor->date());
  EXPECT_EQ("Lavf54.4.100", extractor->encoder());

  EXPECT_EQ(3u, extractor->stream_infos().size());

  EXPECT_EQ("mp3", extractor->stream_infos()[0].type);
  EXPECT_EQ(7u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ("OK Computer",
            GetTagValue(extractor->stream_infos()[0].tags, "album"));
  EXPECT_EQ("Radiohead",
            GetTagValue(extractor->stream_infos()[0].tags, "artist"));
  EXPECT_EQ("1997", GetTagValue(extractor->stream_infos()[0].tags, "date"));
  EXPECT_EQ("Lavf54.4.100",
            GetTagValue(extractor->stream_infos()[0].tags, "encoder"));
  EXPECT_EQ("Alternative",
            GetTagValue(extractor->stream_infos()[0].tags, "genre"));
  EXPECT_EQ("Airbag", GetTagValue(extractor->stream_infos()[0].tags, "title"));
  EXPECT_EQ("1", GetTagValue(extractor->stream_infos()[0].tags, "track"));

  EXPECT_EQ("mp3", extractor->stream_infos()[1].type);
  EXPECT_EQ(0u, extractor->stream_infos()[1].tags.size());

  EXPECT_EQ("png", extractor->stream_infos()[2].type);
  EXPECT_EQ(1u, extractor->stream_infos()[2].tags.size());
  EXPECT_EQ("Other", GetTagValue(extractor->stream_infos()[2].tags, "comment"));

  EXPECT_EQ(1u, extractor->attached_images_bytes().size());
  EXPECT_EQ(155752u, extractor->attached_images_bytes()[0].size());

  EXPECT_EQ("\x89PNG\r\n\x1a\n",
            extractor->attached_images_bytes()[0].substr(0, 8));
  EXPECT_EQ("IEND\xae\x42\x60\x82",
            extractor->attached_images_bytes()[0].substr(
                extractor->attached_images_bytes()[0].size() - 8, 8));
  EXPECT_EQ("\xF3\xED\x8F\xC7\xC7\x98\xB9V|p\xC0u!\xB5\x82\xCF\x95\xF0\xCD\xCE",
            base::SHA1HashString(extractor->attached_images_bytes()[0]));
}

TEST(AudioVideoMetadataExtractorTest, AudioFLACInMp4) {
  std::unique_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("sfx-flac.mp4", true, true, 0.289, -1, -1);
  EXPECT_EQ("Lavf57.75.100", extractor->encoder());

  EXPECT_EQ("mov,mp4,m4a,3gp,3g2,mj2", extractor->stream_infos()[0].type);
  EXPECT_EQ(2u, extractor->stream_infos().size());

  EXPECT_EQ(4u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ("isom",
            GetTagValue(extractor->stream_infos()[0].tags, "major_brand"));
  EXPECT_EQ("512",
            GetTagValue(extractor->stream_infos()[0].tags, "minor_version"));
  EXPECT_EQ("isomiso2mp41", GetTagValue(extractor->stream_infos()[0].tags,
                                        "compatible_brands"));
  EXPECT_EQ("Lavf57.75.100",
            GetTagValue(extractor->stream_infos()[0].tags, "encoder"));

  EXPECT_EQ("flac", extractor->stream_infos()[1].type);
  EXPECT_EQ(2u, extractor->stream_infos()[1].tags.size());
  EXPECT_EQ("SoundHandler",
            GetTagValue(extractor->stream_infos()[1].tags, "handler_name"));
  EXPECT_EQ("und", GetTagValue(extractor->stream_infos()[1].tags, "language"));

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}

}  // namespace media
