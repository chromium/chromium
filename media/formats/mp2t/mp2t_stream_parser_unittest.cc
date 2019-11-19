// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/mp2t_stream_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_pattern.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_HLS_SAMPLE_AES)
#include <openssl/aes.h>
#include <openssl/evp.h>
#include "crypto/openssl_util.h"
#endif

namespace media {
namespace mp2t {

namespace {

bool IsMonotonic(const StreamParser::BufferQueue& buffers) {
  if (buffers.empty())
    return true;

  StreamParser::BufferQueue::const_iterator it1 = buffers.begin();
  StreamParser::BufferQueue::const_iterator it2 = ++it1;
  for ( ; it2 != buffers.end(); ++it1, ++it2) {
    if ((*it2)->GetDecodeTimestamp() < (*it1)->GetDecodeTimestamp())
      return false;
  }
  return true;
}

bool IsAlmostEqual(DecodeTimestamp t0, DecodeTimestamp t1) {
  base::TimeDelta kMaxDeviation = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta diff = t1 - t0;
  return (diff >= -kMaxDeviation && diff <= kMaxDeviation);
}

#if BUILDFLAG(ENABLE_HLS_SAMPLE_AES)
class ScopedCipherCTX {
 public:
  explicit ScopedCipherCTX() { EVP_CIPHER_CTX_init(&ctx_); }
  ~ScopedCipherCTX() {
    EVP_CIPHER_CTX_cleanup(&ctx_);
    crypto::ClearOpenSSLERRStack(FROM_HERE);
  }
  EVP_CIPHER_CTX* get() { return &ctx_; }

 private:
  EVP_CIPHER_CTX ctx_;
};

std::string DecryptSampleAES(const std::string& key,
                             const std::string& iv,
                             const uint8_t* input,
                             int input_size,
                             bool has_pattern) {
  DCHECK(input);
  EXPECT_EQ(input_size % 16, 0);
  crypto::EnsureOpenSSLInit();
  std::string result;
  const EVP_CIPHER* cipher = EVP_aes_128_cbc();
  ScopedCipherCTX ctx;
  EXPECT_EQ(EVP_CipherInit_ex(ctx.get(), cipher, NULL,
                              reinterpret_cast<const uint8_t*>(key.data()),
                              reinterpret_cast<const uint8_t*>(iv.data()), 0),
            1);
  EVP_CIPHER_CTX_set_padding(ctx.get(), 0);
  const size_t output_size = input_size;
  std::unique_ptr<char[]> output(new char[output_size]);
  uint8_t* in_ptr = const_cast<uint8_t*>(input);
  uint8_t* out_ptr = reinterpret_cast<uint8_t*>(output.get());
  size_t bytes_remaining = output_size;

  while (bytes_remaining) {
    int unused;
    size_t amount_to_decrypt = has_pattern ? 16UL : bytes_remaining;
    EXPECT_EQ(amount_to_decrypt % 16UL, 0UL);
    EXPECT_EQ(EVP_CipherUpdate(ctx.get(), out_ptr, &unused, in_ptr,
                               amount_to_decrypt),
              1);
    bytes_remaining -= amount_to_decrypt;
    if (bytes_remaining) {
      out_ptr += amount_to_decrypt;
      in_ptr += amount_to_decrypt;
      size_t amount_to_skip = 144UL;  // Skip 9 blocks.
      if (amount_to_skip > bytes_remaining)
        amount_to_skip = bytes_remaining;
      memcpy(out_ptr, in_ptr, amount_to_skip);
      out_ptr += amount_to_skip;
      in_ptr += amount_to_skip;
      bytes_remaining -= amount_to_skip;
    }
  }

  result.assign(output.get(), output_size);
  return result;
}

// We only support AES-CBC at this time.
// For the purpose of these tests, the key id is also used as the actual key.
std::string DecryptBuffer(const StreamParserBuffer& buffer,
                          EncryptionScheme scheme) {
  EXPECT_EQ(scheme, EncryptionScheme::kCbcs);

  // Audio streams use whole block full sample encryption (so pattern = {0,0}),
  // so only the video stream uses pattern decryption. |has_pattern| is only
  // used by DecryptSampleAES(), which assumes a {1,9} pattern if
  // |has_pattern| = true.
  bool has_pattern =
      buffer.decrypt_config()->encryption_pattern() == EncryptionPattern(1, 9);

  std::string key;
  EXPECT_TRUE(
      LookupTestKeyString(buffer.decrypt_config()->key_id(), false, &key));
  std::string iv = buffer.decrypt_config()->iv();
  EXPECT_EQ(key.size(), 16UL);
  EXPECT_EQ(iv.size(), 16UL);
  std::string result;
  uint8_t* in_ptr = const_cast<uint8_t*>(buffer.data());
  const DecryptConfig* decrypt_config = buffer.decrypt_config();
  for (const auto& subsample : decrypt_config->subsamples()) {
    std::string clear(reinterpret_cast<char*>(in_ptr), subsample.clear_bytes);
    result += clear;
    in_ptr += subsample.clear_bytes;
    result +=
        DecryptSampleAES(key, iv, in_ptr, subsample.cypher_bytes, has_pattern);
    in_ptr += subsample.cypher_bytes;
  }
  return result;
}
#endif

}  // namespace

class Mp2tStreamParserTest : public testing::Test {
 public:
  Mp2tStreamParserTest()
      : segment_count_(0),
        config_count_(0),
        audio_frame_count_(0),
        video_frame_count_(0),
        has_video_(true),
        audio_min_dts_(kNoDecodeTimestamp()),
        audio_max_dts_(kNoDecodeTimestamp()),
        video_min_dts_(kNoDecodeTimestamp()),
        video_max_dts_(kNoDecodeTimestamp()),
        audio_track_id_(0),
        video_track_id_(0),
        current_audio_config_(),
        current_video_config_(),
        capture_buffers(false) {
    bool has_sbr = false;
    parser_.reset(new Mp2tStreamParser(has_sbr));
  }

 protected:
  NullMediaLog media_log_;
  std::unique_ptr<Mp2tStreamParser> parser_;
  int segment_count_;
  int config_count_;
  int audio_frame_count_;
  int video_frame_count_;
  bool has_video_;
  DecodeTimestamp audio_min_dts_;
  DecodeTimestamp audio_max_dts_;
  DecodeTimestamp video_min_dts_;
  DecodeTimestamp video_max_dts_;
  StreamParser::TrackId audio_track_id_;
  StreamParser::TrackId video_track_id_;

  AudioDecoderConfig current_audio_config_;
  VideoDecoderConfig current_video_config_;
  std::vector<scoped_refptr<StreamParserBuffer>> audio_buffer_capture_;
  std::vector<scoped_refptr<StreamParserBuffer>> video_buffer_capture_;
  bool capture_buffers;

  void ResetStats() {
    segment_count_ = 0;
    config_count_ = 0;
    audio_frame_count_ = 0;
    video_frame_count_ = 0;
    audio_min_dts_ = kNoDecodeTimestamp();
    audio_max_dts_ = kNoDecodeTimestamp();
    video_min_dts_ = kNoDecodeTimestamp();
    video_max_dts_ = kNoDecodeTimestamp();
  }

  bool AppendData(const uint8_t* data, size_t length) {
    return parser_->Parse(data, length);
  }

  bool AppendDataInPieces(const uint8_t* data,
                          size_t length,
                          size_t piece_size) {
    const uint8_t* start = data;
    const uint8_t* end = data + length;
    while (start < end) {
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void OnInit(const StreamParser::InitParameters& params) {
    DVLOG(1) << "OnInit: dur=" << params.duration.InMilliseconds();
  }

  bool OnNewConfig(std::unique_ptr<MediaTracks> tracks,
                   const StreamParser::TextTrackConfigMap& tc) {
    DVLOG(1) << "OnNewConfig: got " << tracks->tracks().size() << " tracks";
    bool found_audio_track = false;
    bool found_video_track = false;
    for (const auto& track : tracks->tracks()) {
      const auto& track_id = track->bytestream_track_id();
      if (track->type() == MediaTrack::Audio) {
        audio_track_id_ = track_id;
        found_audio_track = true;
        EXPECT_TRUE(tracks->getAudioConfig(track_id).IsValidConfig());
        current_audio_config_ = tracks->getAudioConfig(track_id);
      } else if (track->type() == MediaTrack::Video) {
        video_track_id_ = track_id;
        found_video_track = true;
        EXPECT_TRUE(tracks->getVideoConfig(track_id).IsValidConfig());
        current_video_config_ = tracks->getVideoConfig(track_id);
      } else {
        // Unexpected track type.
        LOG(ERROR) << "Unexpected track type " << track->type();
        EXPECT_TRUE(false);
      }
    }
    EXPECT_TRUE(found_audio_track);
    EXPECT_EQ(has_video_, found_video_track);
    config_count_++;
    return true;
  }

  void CaptureVideoBuffers(const StreamParser::BufferQueue& video_buffers) {
    for (const auto& buffer : video_buffers) {
      video_buffer_capture_.push_back(buffer);
    }
  }

  void CaptureAudioBuffers(const StreamParser::BufferQueue& audio_buffers) {
    for (const auto& buffer : audio_buffers) {
      audio_buffer_capture_.push_back(buffer);
    }
  }

  bool OnNewBuffers(const StreamParser::BufferQueueMap& buffer_queue_map) {
    EXPECT_GT(config_count_, 0);
    // Ensure that track ids are properly assigned on all emitted buffers.
    for (const auto& it : buffer_queue_map) {
      DVLOG(3) << "Buffers for track_id=" << it.first;
      for (const auto& buf : it.second) {
        DVLOG(3) << "  track_id=" << buf->track_id()
                 << ", size=" << buf->data_size()
                 << ", pts=" << buf->timestamp().InSecondsF()
                 << ", dts=" << buf->GetDecodeTimestamp().InSecondsF()
                 << ", dur=" << buf->duration().InSecondsF();
        EXPECT_EQ(it.first, buf->track_id());
      }
    }

    const StreamParser::BufferQueue empty_buffers;
    const auto& itr_audio = buffer_queue_map.find(audio_track_id_);
    const StreamParser::BufferQueue& audio_buffers =
        (itr_audio == buffer_queue_map.end()) ? empty_buffers
                                              : itr_audio->second;

    const auto& itr_video = buffer_queue_map.find(video_track_id_);
    const StreamParser::BufferQueue& video_buffers =
        (itr_video == buffer_queue_map.end()) ? empty_buffers
                                              : itr_video->second;

    if (capture_buffers) {
      CaptureVideoBuffers(video_buffers);
      CaptureAudioBuffers(audio_buffers);
    }

    // Verify monotonicity.
    if (!IsMonotonic(video_buffers))
      return false;
    if (!IsMonotonic(audio_buffers))
      return false;

    if (!video_buffers.empty()) {
      DecodeTimestamp first_dts = video_buffers.front()->GetDecodeTimestamp();
      DecodeTimestamp last_dts = video_buffers.back()->GetDecodeTimestamp();
      if (video_max_dts_ != kNoDecodeTimestamp() && first_dts < video_max_dts_)
        return false;
      if (video_min_dts_ == kNoDecodeTimestamp())
        video_min_dts_ = first_dts;
      video_max_dts_ = last_dts;
    }
    if (!audio_buffers.empty()) {
      DecodeTimestamp first_dts = audio_buffers.front()->GetDecodeTimestamp();
      DecodeTimestamp last_dts = audio_buffers.back()->GetDecodeTimestamp();
      if (audio_max_dts_ != kNoDecodeTimestamp() && first_dts < audio_max_dts_)
        return false;
      if (audio_min_dts_ == kNoDecodeTimestamp())
        audio_min_dts_ = first_dts;
      audio_max_dts_ = last_dts;
    }

    audio_frame_count_ += audio_buffers.size();
    video_frame_count_ += video_buffers.size();
    return true;
  }

  void OnKeyNeeded(EmeInitDataType type,
                   const std::vector<uint8_t>& init_data) {
#if !BUILDFLAG(ENABLE_HLS_SAMPLE_AES)
    LOG(ERROR) << "OnKeyNeeded not expected in the Mpeg2 TS parser";
    EXPECT_TRUE(false);
#endif
  }

  void OnNewSegment() {
    DVLOG(1) << "OnNewSegment";
    segment_count_++;
  }

  void OnEndOfSegment() {
    LOG(ERROR) << "OnEndOfSegment not expected in the Mpeg2 TS parser";
    EXPECT_TRUE(false);
  }

  void InitializeParser() {
    parser_->Init(
        base::BindOnce(&Mp2tStreamParserTest::OnInit, base::Unretained(this)),
        base::BindRepeating(&Mp2tStreamParserTest::OnNewConfig,
                            base::Unretained(this)),
        base::BindRepeating(&Mp2tStreamParserTest::OnNewBuffers,
                            base::Unretained(this)),
        true,
        base::BindRepeating(&Mp2tStreamParserTest::OnKeyNeeded,
                            base::Unretained(this)),
        base::BindRepeating(&Mp2tStreamParserTest::OnNewSegment,
                            base::Unretained(this)),
        base::BindRepeating(&Mp2tStreamParserTest::OnEndOfSegment,
                            base::Unretained(this)),
        &media_log_);
  }

  bool ParseMpeg2TsFile(const std::string& filename, int append_bytes) {
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                   buffer->data_size(),
                                   append_bytes));
    return true;
  }
};

TEST_F(Mp2tStreamParserTest, UnalignedAppend17) {
  // Test small, non-segment-aligned appends.
  InitializeParser();
  ParseMpeg2TsFile("bear-1280x720.ts", 17);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
  // This stream has no mid-stream configuration change.
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);
}

TEST_F(Mp2tStreamParserTest, UnalignedAppend512) {
  // Test small, non-segment-aligned appends.
  InitializeParser();
  ParseMpeg2TsFile("bear-1280x720.ts", 512);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
  // This stream has no mid-stream configuration change.
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);
}

TEST_F(Mp2tStreamParserTest, AppendAfterFlush512) {
  InitializeParser();
  ParseMpeg2TsFile("bear-1280x720.ts", 512);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);

  ResetStats();
  ParseMpeg2TsFile("bear-1280x720.ts", 512);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);
}

TEST_F(Mp2tStreamParserTest, TimestampWrapAround) {
  // "bear-1280x720_ptswraparound.ts" has been transcoded
  // from bear-1280x720.mp4 by applying a time offset of 95442s
  // (close to 2^33 / 90000) which results in timestamps wrap around
  // in the Mpeg2 TS stream.
  InitializeParser();
  ParseMpeg2TsFile("bear-1280x720_ptswraparound.ts", 512);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);

  EXPECT_TRUE(IsAlmostEqual(video_min_dts_,
                            DecodeTimestamp::FromSecondsD(95443.376)));
  EXPECT_TRUE(IsAlmostEqual(video_max_dts_,
                            DecodeTimestamp::FromSecondsD(95446.079)));

  // Note: for audio, AdtsStreamParser considers only the PTS (which is then
  // used as the DTS).
  // TODO(damienv): most of the time, audio streams just have PTS. Here, only
  // the first PES packet has a DTS, all the other PES packets have PTS only.
  // Reconsider the expected value for |audio_min_dts_| if DTS are used as part
  // of the ADTS stream parser.
  //
  // Note: the last pts for audio is 95445.931 but this PES packet includes
  // 9 ADTS frames with 1 AAC frame in each ADTS frame.
  // So the PTS of the last AAC frame is:
  // 95445.931 + 8 * (1024 / 44100) = 95446.117
  EXPECT_TRUE(IsAlmostEqual(audio_min_dts_,
                            DecodeTimestamp::FromSecondsD(95443.400)));
  EXPECT_TRUE(IsAlmostEqual(audio_max_dts_,
                            DecodeTimestamp::FromSecondsD(95446.117)));
}

TEST_F(Mp2tStreamParserTest, AudioInPrivateStream1) {
  // Test small, non-segment-aligned appends.
  InitializeParser();
  has_video_ = false;
  ParseMpeg2TsFile("bear_adts_in_private_stream_1.ts", 512);
  parser_->Flush();
  EXPECT_EQ(audio_frame_count_, 40);
  EXPECT_EQ(video_frame_count_, 0);
  // This stream has no mid-stream configuration change.
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);
}

#if BUILDFLAG(ENABLE_HLS_SAMPLE_AES)
TEST_F(Mp2tStreamParserTest, HLSSampleAES) {
  std::vector<std::string> decrypted_video_buffers;
  std::vector<std::string> decrypted_audio_buffers;
  InitializeParser();
  capture_buffers = true;
  ParseMpeg2TsFile("bear-1280x720-hls-sample-aes.ts", 2048);
  parser_->Flush();
  EncryptionScheme video_encryption_scheme =
      current_video_config_.encryption_scheme();
  EXPECT_NE(video_encryption_scheme, EncryptionScheme::kUnencrypted);
  for (const auto& buffer : video_buffer_capture_) {
    std::string decrypted_video_buffer =
        DecryptBuffer(*buffer.get(), video_encryption_scheme);
    decrypted_video_buffers.push_back(decrypted_video_buffer);
  }
  EncryptionScheme audio_encryption_scheme =
      current_audio_config_.encryption_scheme();
  EXPECT_NE(audio_encryption_scheme, EncryptionScheme::kUnencrypted);
  for (const auto& buffer : audio_buffer_capture_) {
    std::string decrypted_audio_buffer =
        DecryptBuffer(*buffer.get(), audio_encryption_scheme);
    decrypted_audio_buffers.push_back(decrypted_audio_buffer);
  }

  parser_.reset(new Mp2tStreamParser(false));
  ResetStats();
  InitializeParser();
  video_buffer_capture_.clear();
  audio_buffer_capture_.clear();
  ParseMpeg2TsFile("bear-1280x720-hls.ts", 2048);
  parser_->Flush();
  video_encryption_scheme = current_video_config_.encryption_scheme();
  EXPECT_EQ(video_encryption_scheme, EncryptionScheme::kUnencrypted);
  // Skip the last buffer, which may be truncated.
  for (size_t i = 0; i + 1 < video_buffer_capture_.size(); i++) {
    const auto& buffer = video_buffer_capture_[i];
    std::string unencrypted_video_buffer(
        reinterpret_cast<const char*>(buffer->data()), buffer->data_size());
    EXPECT_EQ(decrypted_video_buffers[i], unencrypted_video_buffer);
  }
  audio_encryption_scheme = current_audio_config_.encryption_scheme();
  EXPECT_EQ(audio_encryption_scheme, EncryptionScheme::kUnencrypted);
  for (size_t i = 0; i + 1 < audio_buffer_capture_.size(); i++) {
    const auto& buffer = audio_buffer_capture_[i];
    std::string unencrypted_audio_buffer(
        reinterpret_cast<const char*>(buffer->data()), buffer->data_size());
    EXPECT_EQ(decrypted_audio_buffers[i], unencrypted_audio_buffer);
  }
}

TEST_F(Mp2tStreamParserTest, PrepareForHLSSampleAES) {
  InitializeParser();
  ParseMpeg2TsFile("bear-1280x720-hls-with-CAT.bin", 2048);
  parser_->Flush();
  EncryptionScheme video_encryption_scheme =
      current_video_config_.encryption_scheme();
  EXPECT_NE(video_encryption_scheme, EncryptionScheme::kUnencrypted);
  EncryptionScheme audio_encryption_scheme =
      current_audio_config_.encryption_scheme();
  EXPECT_NE(audio_encryption_scheme, EncryptionScheme::kUnencrypted);
}

#endif

}  // namespace mp2t
}  // namespace media
