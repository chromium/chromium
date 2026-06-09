// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_file_reader.h"

#include <memory>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/hash.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioFileReaderTest : public testing::TestWithParam<bool> {
 public:
  AudioFileReaderTest() {
    std::vector<base::test::FeatureRef> features = {kDirectOpusAudioDecoding};

#if BUILDFLAG(ENABLE_SYMPHONIA)
    const std::vector<base::test::FeatureRef> symphonia_features = {
        { kSymphoniaAudioDecoding,
          kSymphoniaMp3Decoding,
          kSymphoniaPcmDecoding,
          kSymphoniaVorbisDecoding }};
    features.insert(features.end(), symphonia_features.begin(),
                    symphonia_features.end());
#endif

    if (GetParam()) {
      feature_list_.InitWithFeatures(features,
                                     /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(/*enabled_features=*/{}, features);
    }
  }

  AudioFileReaderTest(const AudioFileReaderTest&) = delete;
  AudioFileReaderTest& operator=(const AudioFileReaderTest&) = delete;

  ~AudioFileReaderTest() override = default;

  void Initialize(const char* filename) {
    filename_ = filename;
    data_ = ReadTestDataFile(filename);
    protocol_ = std::make_unique<InMemoryUrlProtocol>(*data_, false);
    reader_ = std::make_unique<AudioFileReader>(protocol_.get());
  }

  // Reads and validates the entire file provided to Initialize().
  void ReadAndVerify(const char* expected_audio_hash,
                     int expected_frames,
                     double tolerance) {
    std::vector<std::unique_ptr<AudioBus>> decoded_audio_packets;
    const int actual_frames = reader_->Read(&decoded_audio_packets);
    ASSERT_GT(actual_frames, 0);

    std::unique_ptr<AudioBus> decoded_audio_data =
        AudioBus::Create(reader_->channels(), actual_frames);
    int dest_start_frame = 0;
    for (size_t k = 0; k < decoded_audio_packets.size(); ++k) {
      const AudioBus* packet = decoded_audio_packets[k].get();
      const int frame_count = packet->frames();
      packet->CopyPartialFramesTo(0, frame_count, dest_start_frame,
                                  decoded_audio_data.get());
      dest_start_frame += frame_count;
    }
    EXPECT_LE(actual_frames, decoded_audio_data->frames());
    EXPECT_EQ(expected_frames, actual_frames);

    AudioHash audio_hash;
    audio_hash.Update(decoded_audio_data.get(), actual_frames);
    EXPECT_TRUE(audio_hash.IsEquivalent(expected_audio_hash, tolerance))
        << "Audio hashes differ. Expected: " << expected_audio_hash
        << " Actual: " << audio_hash.ToString();
  }

  void ResetReader() {
    ASSERT_TRUE(filename_);
    Initialize(filename_);
    ASSERT_TRUE(reader_->Open());
  }

  // Verify packets are consistent across demuxer runs.  Reads the first few
  // packets and then seeks back to the start timestamp and verifies that the
  // hashes match on the packets just read.
  void VerifyPackets(const char* filename, int packet_reads) {
    auto packet = ScopedAVPacket::Allocate();
    std::vector<std::array<uint8_t, crypto::hash::kSha256Size>> packet_hashes;

    // First, populate the packet hashes.
    for (int i = 0; i < packet_reads; ++i) {
      ASSERT_TRUE(reader_->ReadPacketForTesting(packet.get())) << "i = " << i;
      packet_hashes.push_back(crypto::hash::Sha256(AVPacketData(*packet)));
      av_packet_unref(packet.get());
    }

    // Then re-initialize the reader to seek to the beginning.
    ResetReader();

    // Then validate the hashes.
    for (int i = 0; i < packet_reads; ++i) {
      ASSERT_TRUE(reader_->ReadPacketForTesting(packet.get())) << "i = " << i;
      EXPECT_EQ(base::HexEncode(packet_hashes[i]),
                base::HexEncode(crypto::hash::Sha256(AVPacketData(*packet))));
      av_packet_unref(packet.get());
    }

    // Finally, re-initialize the reader to seek to the beginning again.
    ResetReader();
  }

  void RunTest(const char* fn,
               const char* hash,
               int channels,
               int sample_rate,
               base::TimeDelta duration,
               int frames,
               int expected_frames,
               int packet_reads = 3,
               double tolerance = 0.09) {
    Initialize(fn);
    ASSERT_TRUE(reader_->Open());
    EXPECT_EQ(channels, reader_->channels());
    EXPECT_EQ(sample_rate, reader_->sample_rate());
    if (frames >= 0) {
      EXPECT_EQ(duration.InMicroseconds(),
                reader_->GetDuration().InMicroseconds());
      EXPECT_EQ(frames, reader_->GetNumberOfFrames());
      EXPECT_EQ(reader_->HasKnownDuration(), true);
    } else {
      EXPECT_EQ(reader_->HasKnownDuration(), false);
    }
    if (!packet_verification_disabled_) {
      ASSERT_NO_FATAL_FAILURE(VerifyPackets(fn, packet_reads));
    }
    ReadAndVerify(hash, expected_frames, tolerance);
  }

  void RunTestFailingDemux(const char* fn) {
    Initialize(fn);
    EXPECT_FALSE(reader_->Open());
  }

  void RunTestFailingDecode(const char* fn) {
    Initialize(fn);
    EXPECT_TRUE(reader_->Open());
    std::vector<std::unique_ptr<AudioBus>> decoded_audio_packets;
    EXPECT_EQ(reader_->Read(&decoded_audio_packets), 0u);
  }

  void RunTestPartialDecode(const char* fn) {
    Initialize(fn);
    EXPECT_TRUE(reader_->Open());
    std::vector<std::unique_ptr<AudioBus>> decoded_audio_packets;
    constexpr int packets_to_read = 1;
    reader_->Read(&decoded_audio_packets, packets_to_read);
    EXPECT_EQ(static_cast<int>(decoded_audio_packets.size()), packets_to_read);
  }

  void disable_packet_verification() { packet_verification_disabled_ = true; }

 protected:
  base::test::ScopedFeatureList feature_list_;
  const char* filename_ = nullptr;
  scoped_refptr<DecoderBuffer> data_;
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  std::unique_ptr<AudioFileReader> reader_;
  bool packet_verification_disabled_ = false;
};

// Note: When adding new test files it's important to double check the total
// number of samples emitted in comparison to the corresponding reference
// decoder. I.e.,
//   * For opus, get opus-tools, and use the opusdec command line tool.
//   * For vorbis, get ogg-tools, and use the oggdec command line tool.
//
// These will each decode to a wave file. Given a wave file you can run:
//   * ffprobe -show_packets $wave_file | tail -10
//
// Adding the last `pts` and `duration` values together will give you an
// official total sample count.

TEST_P(AudioFileReaderTest, WithoutOpen) {
  Initialize("bear.ogv");
}

TEST_P(AudioFileReaderTest, InvalidFile) {
  RunTestFailingDemux("ten_byte_file");
}

TEST_P(AudioFileReaderTest, UnknownDuration) {
  RunTest("bear-320x240-live.webm", "-3.59,-2.06,-0.43,2.15,0.77,-0.95,", 2,
          44100, base::Microseconds(-1), -1, 121024);
}

TEST_P(AudioFileReaderTest, WithVideo) {
  // The total samples should be 45568 after applying discard padding.
  // ffprobe shows discard_padding=64 for the last packet.
  RunTest("bear.ogv", "-2.10,-1.01,0.24,1.48,0.70,-0.68,", 2, 44100,
          base::Microseconds(1011520), 44609, 45568);
}

TEST_P(AudioFileReaderTest, FLAC) {
  RunTest("sfx.flac", "3.03,2.86,2.99,3.31,3.57,4.06,", 1, 44100,
          base::Microseconds(288414), 12720, 12719);
}

TEST_P(AudioFileReaderTest, FLAC_WithMask) {
  RunTest("with_mask.flac", "9.04,3.45,11.95,4.69,10.32,2.10,", 4, 44100,
          base::Microseconds(1875012), 82689, 82688);
}

TEST_P(AudioFileReaderTest, Vorbis) {
  // oggdec produces 15435 samples, but the last 629 samples are silence
  // corresponding to discard padding. FFmpeg applies this discard padding,
  // resulting in 15307 samples.
  RunTest("sfx.ogg", "3.95,3.02,3.98,5.14,5.65,5.13,", 1, 44100,
          base::Microseconds(350001), 15436, 15307);
}

TEST_P(AudioFileReaderTest, WaveU8) {
  RunTest("sfx_u8.wav", "-1.23,-1.57,-1.14,-0.91,-0.87,-0.07,", 1, 44100,
          base::Microseconds(288414), 12720, 12719);
}

TEST_P(AudioFileReaderTest, WaveS16LE) {
  RunTest("sfx_s16le.wav", "3.05,2.87,3.00,3.32,3.58,4.08,", 1, 44100,
          base::Microseconds(288414), 12720, 12719);
}

TEST_P(AudioFileReaderTest, WaveS24LE) {
  RunTest("sfx_s24le.wav", "3.03,2.86,2.99,3.31,3.57,4.06,", 1, 44100,
          base::Microseconds(288414), 12720, 12719);
}

TEST_P(AudioFileReaderTest, WaveF32LE) {
  RunTest("sfx_f32le.wav", "3.03,2.86,2.99,3.31,3.57,4.06,", 1, 44100,
          base::Microseconds(288414), 12720, 12719);
}

TEST_P(AudioFileReaderTest, MP3) {
  RunTest("sfx.mp3", "1.30,2.72,4.56,5.08,3.74,2.03,", 1, 44100,
          base::Microseconds(250001), 11026, 11025);
}

TEST_P(AudioFileReaderTest, CorruptMP3) {
  // Disable packet verification since the file is corrupt and FFmpeg does not
  // make any guarantees on packet consistency in this case.
  disable_packet_verification();
  RunTest("corrupt.mp3", "-4.95,-2.95,-0.44,1.16,0.31,-2.21,", 1, 44100,
          base::Microseconds(1018801), 44930, 44928);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_P(AudioFileReaderTest, AAC) {
  RunTest("sfx.m4a", "2.47,2.30,2.45,2.80,3.06,3.56,", 1, 44100,
          base::Microseconds(347665), 15333, 12719);
}

TEST_P(AudioFileReaderTest, AAC_SinglePacket) {
  RunTest("440hz-10ms.m4a", "3.77,4.53,4.75,3.48,3.67,3.76,", 1, 44100,
          base::Microseconds(69660), 3073, 441);
}

TEST_P(AudioFileReaderTest, AAC_TinyClipRepro) {
  RunTest("tiny-clip.mp4", "-1.77,1.15,-5.82,-1.21,-3.43,2.93,", 2, 48000,
          base::Microseconds(680042), 32643, 30011);
}

TEST_P(AudioFileReaderTest, AAC_ADTS) {
  RunTest("sfx.adts", "1.80,1.66,2.31,3.26,4.46,3.36,", 1, 44100,
          base::Microseconds(384733), 16967, 13312);
}

TEST_P(AudioFileReaderTest, AAC_ObsRemuxVariablePacketDurations) {
  Initialize("obs_remux_variable_aac_durations.mp4");
  ASSERT_TRUE(reader_->Open());
  EXPECT_EQ(2, reader_->channels());
  EXPECT_EQ(48000, reader_->sample_rate());

  // Regression test for OBS-remuxed MP4 audio with alternating AAC packet
  // durations. Intermediate decoded buffers must not be tail-trimmed based on
  // these packet durations.
  ReadAndVerify("-3.33,-0.33,-0.34,0.42,-1.97,-1.96,", 301040, 0.09);
}

TEST_P(AudioFileReaderTest, MidStreamConfigChangesFail) {
  RunTestFailingDecode("midstream_config_change.mp3");
}
#endif

TEST_P(AudioFileReaderTest, VorbisValidChannelLayout) {
  if (GetParam()) {
    // TODO(crbug.com/495575937): Remove this check once Symphonia supports 9
    // channels.
    Initialize("9ch.ogg");
    EXPECT_FALSE(reader_->Open());
    return;
  }
  // The total samples should be 4800 after applying discard padding.
  RunTest("9ch.ogg", "102.08,12.51,57.91,56.94,63.05,17.30,", 9, 48000,
          base::Microseconds(100001), 4801, 4800);
}

TEST_P(AudioFileReaderTest, WaveValidFourChannelLayout) {
  RunTest("4ch.wav", "131.71,38.02,130.31,44.89,135.98,42.52,", 4, 44100,
          base::Microseconds(100001), 4411, 4410, /*packet_reads=*/2);
}

TEST_P(AudioFileReaderTest, ReadPartialMP3) {
  RunTestPartialDecode("sfx.mp3");
}

TEST_P(AudioFileReaderTest, OpusOutputsF32Samples) {
  RunTest("bear-opus.ogg", "3.64,0.30,0.34,-0.25,1.70,1.68,", 2, 48000,
          base::Microseconds(2767022), 132818, 132169);
}

TEST_P(AudioFileReaderTest, OpusTrimmingTestMp4) {
  RunTest("opus-trimming-test.mp4", "-7.27,-6.96,-5.99,-5.58,-5.66,-6.27,", 1,
          48000, base::Microseconds(12840001), 616321, 550785);
}

TEST_P(AudioFileReaderTest, OpusTrimmingTestOgg) {
  // Hash should match PipelineIntegrationTest::kOpusEndTrimmingHash_1
  RunTest("opus-trimming-test.ogg", "-4.57,-5.67,-6.52,-6.28,-4.34,-3.58,", 1,
          48000, base::Microseconds(12720022), 610562, 545026);
}

TEST_P(AudioFileReaderTest, OpusTrimmingTestWebm) {
  // Hash should match PipelineIntegrationTest::kOpusEndTrimmingHash_1
  RunTest("opus-trimming-test.webm", "-4.57,-5.67,-6.52,-6.28,-4.34,-3.58,", 1,
          48000, base::Microseconds(12720001), 610561, 545026);
}

TEST_P(AudioFileReaderTest, OpusDecodeTest) {
  RunTest("opus-test.opus", "0.67,-0.92,4.13,1.95,4.16,-1.02,", 2, 48000,
          base::Microseconds(1016480), 48792, 48479);
}

// If Symphonia build support is enabled, test with both the Symphonia
// audio decoder feature enabled and disabled. Otherwise, just provide a false
// parameter so no duplicate tests are ran.
INSTANTIATE_TEST_SUITE_P(All,
                         AudioFileReaderTest,
#if BUILDFLAG(ENABLE_SYMPHONIA)
                         ::testing::Bool()
#else
                         ::testing::Values(false)
#endif
);

}  // namespace media
