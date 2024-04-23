// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_file_reader.h"

#include <memory>
#include <string_view>

#include "base/hash/md5.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioFileReaderTest : public testing::Test {
 public:
  AudioFileReaderTest() : packet_verification_disabled_(false) {}

  AudioFileReaderTest(const AudioFileReaderTest&) = delete;
  AudioFileReaderTest& operator=(const AudioFileReaderTest&) = delete;

  ~AudioFileReaderTest() override = default;

  void Initialize(const char* filename) {
    data_ = ReadTestDataFile(filename);
    protocol_ = std::make_unique<InMemoryUrlProtocol>(data_->data(),
                                                      data_->size(), false);
    reader_ = std::make_unique<AudioFileReader>(protocol_.get());
  }

  // Reads and the entire file provided to Initialize().
  void ReadAndVerify(const char* expected_audio_hash, int expected_frames) {
    std::vector<std::unique_ptr<AudioBus>> decoded_audio_packets;
    int actual_frames = reader_->Read(&decoded_audio_packets);
    std::unique_ptr<AudioBus> decoded_audio_data =
        AudioBus::Create(reader_->channels(), actual_frames);
    int dest_start_frame = 0;
    for (size_t k = 0; k < decoded_audio_packets.size(); ++k) {
      const AudioBus* packet = decoded_audio_packets[k].get();
      int frame_count = packet->frames();
      packet->CopyPartialFramesTo(0, frame_count, dest_start_frame,
                                  decoded_audio_data.get());
      dest_start_frame += frame_count;
    }
    ASSERT_LE(actual_frames, decoded_audio_data->frames());
    ASSERT_EQ(expected_frames, actual_frames);

    AudioHash audio_hash;
    audio_hash.Update(decoded_audio_data.get(), actual_frames);
    EXPECT_EQ(expected_audio_hash, audio_hash.ToString());
  }

  // Verify packets are consistent across demuxer runs.  Reads the first few
  // packets and then seeks back to the start timestamp and verifies that the
  // hashes match on the packets just read.
  void VerifyPackets(int packet_reads) {
    const int kTestPasses = 2;

    auto packet = ScopedAVPacket::Allocate();
    base::TimeDelta start_timestamp;
    std::vector<std::string> packet_md5_hashes_;
    for (int i = 0; i < kTestPasses; ++i) {
      for (int j = 0; j < packet_reads; ++j) {
        ASSERT_TRUE(reader_->ReadPacketForTesting(packet.get()));

        // On the first pass save the MD5 hash of each packet, on subsequent
        // passes ensure it matches.
        const std::string md5_hash = base::MD5String(std::string_view(
            reinterpret_cast<char*>(packet->data), packet->size));
        if (i == 0) {
          packet_md5_hashes_.push_back(md5_hash);
          if (j == 0) {
            start_timestamp = ConvertFromTimeBase(
                reader_->codec_context_for_testing()->time_base, packet->pts);
          }
        } else {
          EXPECT_EQ(packet_md5_hashes_[j], md5_hash) << "j = " << j;
        }

        av_packet_unref(packet.get());
      }
      ASSERT_TRUE(reader_->SeekForTesting(start_timestamp));
    }
  }

  void RunTest(const char* fn,
               const char* hash,
               int channels,
               int sample_rate,
               base::TimeDelta duration,
               int frames,
               int expected_frames,
               int packet_reads = 3) {
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
    if (!packet_verification_disabled_)
      ASSERT_NO_FATAL_FAILURE(VerifyPackets(packet_reads));
    ReadAndVerify(hash, expected_frames);
  }

  void RunTestFailingDemux(const char* fn) {
    Initialize(fn);
    EXPECT_FALSE(reader_->Open());
  }

  void RunTestFailingDecode(const char* fn, int expect_read = 0) {
    Initialize(fn);
    EXPECT_TRUE(reader_->Open());
    std::vector<std::unique_ptr<AudioBus>> decoded_audio_packets;
    EXPECT_EQ(reader_->Read(&decoded_audio_packets), expect_read);
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
  scoped_refptr<DecoderBuffer> data_;
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  std::unique_ptr<AudioFileReader> reader_;
  bool packet_verification_disabled_;
};

TEST_F(AudioFileReaderTest, WithoutOpen) {
  Initialize("bear.ogv");
}

TEST_F(AudioFileReaderTest, InvalidFile) {
  RunTestFailingDemux("ten_byte_file");
}

TEST_F(AudioFileReaderTest, UnknownDuration) {
  RunTest("bear-320x240-live.webm", "-3.59,-2.06,-0.43,2.15,0.77,-0.95,", 2,
          44100, base::Microseconds(-1), -1, 121024);
}

TEST_F(AudioFileReaderTest, WithVideo) {
  RunTest("bear.ogv", "-0.73,0.92,0.48,-0.07,-0.92,-0.88,", 2, 44100,
          base::Microseconds(1011520), 44609, 45632);
}

TEST_F(AudioFileReaderTest, Vorbis) {
  RunTest("sfx.ogg", "2.17,3.31,5.15,6.33,5.97,4.35,", 1, 44100,
          base::Microseconds(350001), 15436, 15936);
}

TEST_F(AudioFileReaderTest, WaveU8) {
  RunTest("sfx_u8.wav", "-1.23,-1.57,-1.14,-0.91,-0.87,-0.07,", 1, 44100,
          base::Microseconds(288414), 12720, 12719);
}

TEST_F(AudioFileReaderTest, WaveS16LE) {
  RunTest("sfx_s16le.wav", "3.05,2.87,3.00,3.32,3.58,4.08,", 1, 44100,
          base::Microseconds(288414), 12720, 12719);
}

TEST_F(AudioFileReaderTest, WaveS24LE) {
  RunTest("sfx_s24le.wav", "3.03,2.86,2.99,3.31,3.57,4.06,", 1, 44100,
          base::Microseconds(288414), 12720, 12719);
}

TEST_F(AudioFileReaderTest, WaveF32LE) {
  RunTest("sfx_f32le.wav", "3.03,2.86,2.99,3.31,3.57,4.06,", 1, 44100,
          base::Microseconds(288414), 12720, 12719);
}

TEST_F(AudioFileReaderTest, MP3) {
  RunTest("sfx.mp3", "1.30,2.72,4.56,5.08,3.74,2.03,", 1, 44100,
          base::Microseconds(313470), 13825, 11025);
}

TEST_F(AudioFileReaderTest, CorruptMP3) {
  // Disable packet verification since the file is corrupt and FFmpeg does not
  // make any guarantees on packet consistency in this case.
  disable_packet_verification();
  RunTest("corrupt.mp3", "-4.95,-2.95,-0.44,1.16,0.31,-2.21,", 1, 44100,
          base::Microseconds(1018801), 44930, 44928);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_F(AudioFileReaderTest, AAC) {
  RunTest("sfx.m4a", "0.79,2.31,4.15,4.92,4.04,1.44,", 1, 44100,
          base::Microseconds(347665), 15333, 12701);
}

TEST_F(AudioFileReaderTest, AAC_SinglePacket) {
  RunTest("440hz-10ms.m4a", "3.77,4.53,4.75,3.48,3.67,3.76,", 1, 44100,
          base::Microseconds(69660), 3073, 441);
}

TEST_F(AudioFileReaderTest, AAC_ADTS) {
  RunTest("sfx.adts", "1.80,1.66,2.31,3.26,4.46,3.36,", 1, 44100,
          base::Microseconds(384733), 16967, 13312);
}

TEST_F(AudioFileReaderTest, MidStreamConfigChangesFail) {
  RunTestFailingDecode("midstream_config_change.mp3", 0);
}
#endif

TEST_F(AudioFileReaderTest, VorbisInvalidChannelLayout) {
  RunTestFailingDemux("9ch.ogg");
}

TEST_F(AudioFileReaderTest, WaveValidFourChannelLayout) {
  RunTest("4ch.wav", "131.71,38.02,130.31,44.89,135.98,42.52,", 4, 44100,
          base::Microseconds(100001), 4411, 4410, /*packet_reads=*/2);
}

TEST_F(AudioFileReaderTest, ReadPartialMP3) {
  RunTestPartialDecode("sfx.mp3");
}

}  // namespace media
