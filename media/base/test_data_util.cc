// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/test_data_util.h"

#include <stdint.h>
#include <ostream>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"

namespace media {

namespace {

// Mime types for test files. Sorted in the ASCII code order of the variable
// names.
const char kAacAdtsAudio[] = "audio/aac";
const char kMp2AudioSBR[] = "video/mp2t; codecs=\"avc1.4D4041,mp4a.40.5\"";
const char kMp2tAudioVideo[] = "video/mp2t; codecs=\"mp4a.40.2, avc1.42E01E\"";
const char kMp3Audio[] = "audio/mpeg";
// MP4
const char kMp4AacAudio[] = "audio/mp4; codecs=\"mp4a.40.2\"";
const char kMp4Av110bitVideo[] = "video/mp4; codecs=\"av01.0.04M.10\"";
const char kMp4Av1Video[] = "video/mp4; codecs=\"av01.0.04M.08\"";
const char kMp4Av1VideoOpusAudio[] = "video/mp4; codecs=\"av01.0.04M.08,opus\"";
const char kMp4Avc1Video[] = "video/mp4; codecs=\"avc1.64001E\"";
const char kMp4AacAudioAvc1Video[] =
    "video/mp4; codecs=\"mp4a.40.2, avc1.64001E\"";
const char kMp4Avc3Video[] = "video/mp4; codecs=\"avc3.64001f\"";
const char kMp4FlacAudio[] = "audio/mp4; codecs=\"flac\"";
const char kMp4OpusAudio[] = "audio/mp4; codecs=\"opus\"";
const char kMp4Vp9Profile2Video[] =
    "video/mp4; codecs=\"vp09.02.10.10.01.02.02.02.00\"";
const char kMp4Vp9Video[] =
    "video/mp4; codecs=\"vp09.00.10.08.01.02.02.02.00\"";
const char kMp4XheAacAudio[] = "audio/mp4; codecs=\"mp4a.40.42\"";
const char kMp4DolbyVisionProfile5[] = "video/mp4; codecs=\"dvh1.05.06\"";
const char kMp4DolbyVisionProfile8x[] = "video/mp4; codecs=\"dvhe.08.07\"";
// WebM
const char kWebMAv110bitVideo[] = "video/webm; codecs=\"av01.0.04M.10\"";
const char kWebMAv1Video[] = "video/webm; codecs=\"av01.0.04M.08\"";
const char kWebMOpusAudio[] = "audio/webm; codecs=\"opus\"";
const char kWebMOpusAudioVp9Video[] = "video/webm; codecs=\"opus, vp9\"";
const char kWebMVorbisAudio[] = "audio/webm; codecs=\"vorbis\"";
const char kWebMVorbisAudioVp8Video[] = "video/webm; codecs=\"vorbis, vp8\"";
const char kWebMVp8Video[] = "video/webm; codecs=\"vp8\"";
const char kWebMVp9Profile2Video[] =
    "video/webm; codecs=\"vp09.02.10.10.01.02.02.02.00\"";
const char kWebMVp9Video[] = "video/webm; codecs=\"vp9\"";

// A map from a test file name to its mime type. The file is located at
// media/test/data.
using FileToMimeTypeMap = base::flat_map<std::string, std::string>;

// Wrapped to avoid static initializer startup cost. The list is sorted in the
// the ASCII code order of file names.
// Note: Some files are old and the codec string in the mime type may not be
// accurate.
// Warning: When adding new files, make sure the codec string is accurate. For
// example kMp4Avc1Video is for H264 high profile. If you add a file that uses
// main profile, a new mime type should be added.
const FileToMimeTypeMap& GetFileToMimeTypeMap() {
  static const base::NoDestructor<FileToMimeTypeMap> kFileToMimeTypeMap({
      {"bear-1280x720-a_frag-cenc-key_rotation.mp4", kMp4AacAudio},
      {"bear-1280x720-a_frag-cenc.mp4", kMp4AacAudio},
      {"bear-1280x720-a_frag-cenc_clear-all.mp4", kMp4AacAudio},
      {"bear-1280x720-aac_he.ts", kMp2AudioSBR},
      {"bear-1280x720-v_frag-avc3.mp4", kMp4Avc3Video},
      {"bear-1280x720-v_frag-cenc-key_rotation.mp4", kMp4Avc1Video},
      {"bear-1280x720-v_frag-cenc.mp4", kMp4Avc1Video},
      {"bear-1280x720-v_frag-cenc_clear-all.mp4", kMp4Avc1Video},
      {"bear-1280x720.ts", kMp2tAudioVideo},
      {"bear-320x240-16x9-aspect-av_enc-av.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-16x9-aspect.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-audio-only.webm", kWebMVorbisAudio},
      {"bear-320x240-av_enc-a.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-av_enc-av.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-av_enc-av_clear-1s.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-av_enc-av_clear-all.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-av_enc-v.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-live.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-opus-a_enc-a.webm", kWebMOpusAudio},
      {"bear-320x240-opus-av_enc-av.webm", kWebMOpusAudioVp9Video},
      {"bear-320x240-opus-av_enc-v.webm", kWebMOpusAudioVp9Video},
      {"bear-320x240-v-vp9_fullsample_enc-v.webm", kWebMVp9Video},
      {"bear-320x240-v-vp9_profile2_subsample_cenc-v.mp4",
       kMp4Vp9Profile2Video},
      {"bear-320x240-v-vp9_profile2_subsample_cenc-v.webm",
       kWebMVp9Profile2Video},
      {"bear-320x240-v-vp9_subsample_enc-v.webm", kWebMVp9Video},
      {"bear-320x240-v_enc-v.webm", kWebMVp8Video},
      {"bear-320x240-v_frag-vp9-cenc.mp4", kMp4Vp9Video},
      {"bear-320x240-v_frag-vp9.mp4", kMp4Vp9Video},
      {"bear-320x240-video-only.webm", kWebMVp8Video},
      {"bear-320x240.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240_corrupted_after_init_segment.webm",
       kWebMVorbisAudioVp8Video},
      {"bear-640x360-a_frag-cbcs.mp4", kMp4AacAudio},
      {"bear-640x360-a_frag-cenc.mp4", kMp4AacAudio},
      {"bear-640x360-a_frag-cenc.mp4;bear-640x360-v_frag-cenc.mp4",
       kMp4AacAudioAvc1Video},
      {"bear-640x360-a_frag.mp4", kMp4AacAudio},
      {"bear-640x360-av_frag.mp4", kMp4AacAudioAvc1Video},
      {"bear-640x360-v_frag-cbc1.mp4", kMp4Avc1Video},
      {"bear-640x360-v_frag-cbcs.mp4", kMp4Avc1Video},
      {"bear-640x360-v_frag-cenc-key_rotation.mp4", kMp4Avc1Video},
      {"bear-640x360-v_frag-cenc-mdat.mp4", kMp4Avc1Video},
      {"bear-640x360-v_frag-cenc-senc-no-saiz-saio.mp4", kMp4Avc1Video},
      {"bear-640x360-v_frag-cenc-senc.mp4", kMp4Avc1Video},
      {"bear-640x360-v_frag-cenc.mp4", kMp4Avc1Video},
      {"bear-640x360-v_frag-cens.mp4", kMp4Avc1Video},
      {"bear-640x360-v_frag.mp4", kMp4Avc1Video},
      {"bear-a_enc-a.webm", kWebMVorbisAudio},
      {"bear-audio-implicit-he-aac-v1.aac", kAacAdtsAudio},
      {"bear-audio-implicit-he-aac-v2.aac", kAacAdtsAudio},
      {"bear-audio-lc-aac.aac", kAacAdtsAudio},
      {"bear-audio-main-aac.aac", kAacAdtsAudio},
      {"bear-audio-mp4a.69.ts", "video/mp2t; codecs=\"mp4a.69\""},
      {"bear-audio-mp4a.6B.ts", "video/mp2t; codecs=\"mp4a.6B\""},
      {"bear-av1-320x180-10bit-cenc.mp4", kMp4Av110bitVideo},
      {"bear-av1-320x180-10bit-cenc.webm", kWebMAv110bitVideo},
      {"bear-av1-320x180-10bit.mp4", kMp4Av110bitVideo},
      {"bear-av1-320x180-10bit.webm", kWebMAv110bitVideo},
      {"bear-av1-480x360.webm", kWebMAv1Video},
      {"bear-av1-cenc.mp4", kMp4Av1Video},
      {"bear-av1-cenc.webm", kWebMAv1Video},
      {"bear-av1-opus.mp4", kMp4Av1VideoOpusAudio},
      {"bear-av1.mp4", kMp4Av1Video},
      {"bear-av1.webm", kWebMAv1Video},
      {"bear-flac-cenc.mp4", kMp4FlacAudio},
      {"bear-flac_frag.mp4", kMp4FlacAudio},
      {"bear-opus.mp4", kMp4OpusAudio},
      {"bear-opus.webm", kWebMOpusAudio},
      {"bear-opus-cenc.mp4", kMp4OpusAudio},
      {"bear-vp8a.webm", kWebMVp8Video},
      {"bear-vp9-blockgroup.webm", kWebMVp9Video},
      {"bear-vp9.webm", kWebMVp9Video},
      {"color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag-cenc.mp4",
       kMp4DolbyVisionProfile5},
      {"color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag-cenc-clearlead-"
       "2sec.mp4",
       kMp4DolbyVisionProfile5},
      {"color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag-"
       "cenc.mp4",
       kMp4DolbyVisionProfile8x},
      {"color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag-"
       "cenc-clearlead-2sec.mp4",
       kMp4DolbyVisionProfile8x},
      {"frame_size_change-av_enc-v.webm", kWebMVorbisAudioVp8Video},
      {"icy_sfx.mp3", kMp3Audio},
      {"noise-xhe-aac.mp4", kMp4XheAacAudio},
      {"opus-trimming-test.mp4", kMp4OpusAudio},
      {"opus-trimming-test.webm", kWebMOpusAudio},
      {"sfx-flac_frag.mp4", kMp4FlacAudio},
      {"sfx-opus-441.webm", kWebMOpusAudio},
      {"sfx-opus_frag.mp4", kMp4OpusAudio},
      {"sfx.adts", kAacAdtsAudio},
      {"sfx.mp3", kMp3Audio},
  });

  return *kFileToMimeTypeMap;
}

// Key used to encrypt test files.
const uint8_t kSecretKey[] = {0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
                              0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c};

// The key ID for all encrypted files.
const uint8_t kKeyId[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                          0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};

}  // namespace

// TODO(sandersd): Change the tests to use a more unique message.
// See http://crbug.com/592067

// Common test results.
const char kFailedTitle[] = "FAILED";

// Upper case event name set by Utils.installTitleEventHandler().
const char kEndedTitle[] = "ENDED";
const char kErrorEventTitle[] = "ERROR";

// Lower case event name as set by Utils.failTest().
const char kErrorTitle[] = "error";

const base::FilePath::CharType kTestDataPath[] =
    FILE_PATH_LITERAL("media/test/data");

const base::span<const uint8_t> ExternalMemoryAdapterForTesting::Span() const {
  return span_;
}

base::FilePath GetTestDataFilePath(std::string_view name) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path));
  return file_path.Append(GetTestDataPath()).AppendASCII(name);
}

base::FilePath GetTestDataPath() {
  return base::FilePath(kTestDataPath);
}

std::string GetMimeTypeForFile(std::string_view file_name) {
  const auto& map = GetFileToMimeTypeMap();
  auto itr = map.find(file_name);
  CHECK(itr != map.end()) << ": file_name = " << file_name;
  return itr->second;
}

std::string GetURLQueryString(const base::StringPairs& query_params) {
  std::string query = "";
  auto itr = query_params.begin();
  for (; itr != query_params.end(); ++itr) {
    if (itr != query_params.begin())
      query.append("&");
    query.append(itr->first + "=" + itr->second);
  }
  return query;
}

scoped_refptr<DecoderBuffer> ReadTestDataFile(std::string_view name) {
  base::FilePath file_path = GetTestDataFilePath(name);

  int64_t tmp = 0;
  CHECK(base::GetFileSize(file_path, &tmp))
      << "Failed to get file size for '" << name << "'";

  int file_size = base::checked_cast<int>(tmp);

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(file_size));
  auto* data = reinterpret_cast<char*>(buffer->writable_data());
  CHECK_EQ(file_size, base::ReadFile(file_path, data, file_size))
      << "Failed to read '" << name << "'";

  return buffer;
}

scoped_refptr<DecoderBuffer> ReadTestDataFile(std::string_view name,
                                              base::TimeDelta pts) {
  auto buffer = ReadTestDataFile(name);
  buffer->set_timestamp(pts);
  return buffer;
}

bool LookupTestKeyVector(const std::vector<uint8_t>& key_id,
                         bool allow_rotation,
                         std::vector<uint8_t>* key) {
  std::vector<uint8_t> starting_key_id(kKeyId, kKeyId + std::size(kKeyId));
  size_t rotate_limit = allow_rotation ? starting_key_id.size() : 1;
  for (size_t pos = 0; pos < rotate_limit; ++pos) {
    std::rotate(starting_key_id.begin(), starting_key_id.begin() + pos,
                starting_key_id.end());
    if (key_id == starting_key_id) {
      key->assign(kSecretKey, kSecretKey + std::size(kSecretKey));
      std::rotate(key->begin(), key->begin() + pos, key->end());
      return true;
    }
  }
  return false;
}

bool LookupTestKeyString(std::string_view key_id,
                         bool allow_rotation,
                         std::string* key) {
  std::vector<uint8_t> key_vector;
  bool result =
      LookupTestKeyVector(std::vector<uint8_t>(key_id.begin(), key_id.end()),
                          allow_rotation, &key_vector);
  if (result)
    *key = std::string(key_vector.begin(), key_vector.end());
  return result;
}

}  // namespace media
