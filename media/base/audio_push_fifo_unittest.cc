// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <limits>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_push_fifo.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class AudioPushFifoTest : public testing::TestWithParam<int> {
 public:
  AudioPushFifoTest() = default;

  AudioPushFifoTest(const AudioPushFifoTest&) = delete;
  AudioPushFifoTest& operator=(const AudioPushFifoTest&) = delete;

  ~AudioPushFifoTest() override = default;

  int output_chunk_size() const { return GetParam(); }

  void SetUp() final {
    fifo_ = std::make_unique<AudioPushFifo>(base::BindRepeating(
        &AudioPushFifoTest::ReceiveAndCheckNextChunk, base::Unretained(this)));
    fifo_->Reset(output_chunk_size());
    ASSERT_EQ(output_chunk_size(), fifo_->frames_per_buffer());
  }

 protected:
  struct OutputChunkResult {
    int num_frames;
    float first_sample_value;
    float last_sample_value;
    int frame_delay;
  };

  // Returns the number of output chunks that should have been emitted given the
  // number of input frames pushed so far.
  size_t GetExpectedOutputChunks(int frames_pushed) const {
    return static_cast<size_t>(frames_pushed / output_chunk_size());
  }

  // Returns the number of Push() calls to make in order to get at least 3
  // output chunks.
  int GetNumPushTestIterations(int input_chunk_size) const {
    return 3 * std::max(1, output_chunk_size() / input_chunk_size);
  }

  // Repeatedly pushes constant-sized batches of input samples and checks that
  // the input data is re-chunked correctly.
  void RunSimpleRechunkTest(int input_chunk_size) {
    const int num_iterations = GetNumPushTestIterations(input_chunk_size);

    int sample_value = 0;
    const std::unique_ptr<AudioBus> audio_bus =
        AudioBus::Create(1, input_chunk_size);

    for (int i = 0; i < num_iterations; ++i) {
      EXPECT_EQ(GetExpectedOutputChunks(i * input_chunk_size), results_.size());

      // Fill audio data with predictable values.
      for (int j = 0; j < audio_bus->frames(); ++j)
        audio_bus->channel(0)[j] = static_cast<float>(sample_value++);

      fifo_->Push(*audio_bus);
      // Note: AudioPushFifo has just called ReceiveAndCheckNextChunk() zero or
      // more times.
    }
    EXPECT_EQ(GetExpectedOutputChunks(num_iterations * input_chunk_size),
              results_.size());

    // Confirm first and last sample values that have been output are the
    // expected ones.
    ASSERT_FALSE(results_.empty());
    EXPECT_EQ(0.0f, results_.front().first_sample_value);
    const float last_value_in_last_chunk = static_cast<float>(
        GetExpectedOutputChunks(num_iterations * input_chunk_size) *
            output_chunk_size() -
        1);
    EXPECT_EQ(last_value_in_last_chunk, results_.back().last_sample_value);

    // Confirm the expected frame delays for the first output chunk (or two).
    if (input_chunk_size < output_chunk_size()) {
      const int num_queued_before_first_output =
          ((output_chunk_size() - 1) / input_chunk_size) * input_chunk_size;
      EXPECT_EQ(-num_queued_before_first_output, results_.front().frame_delay);
    } else if (input_chunk_size >= output_chunk_size()) {
      EXPECT_EQ(0, results_[0].frame_delay);
      if (input_chunk_size >= 2 * output_chunk_size()) {
        EXPECT_EQ(output_chunk_size(), results_[1].frame_delay);
      } else {
        const int num_remaining_after_first_output =
            input_chunk_size - output_chunk_size();
        EXPECT_EQ(-num_remaining_after_first_output, results_[1].frame_delay);
      }
    }

    const size_t num_results_before_flush = results_.size();
    fifo_->Flush();
    const size_t num_results_after_flush = results_.size();
    if (num_results_after_flush > num_results_before_flush) {
      EXPECT_NE(0, results_.back().frame_delay);
      EXPECT_LT(-output_chunk_size(), results_.back().frame_delay);
    }
  }

  // Returns a "random" integer in the range [begin,end).
  int GetRandomInRange(int begin, int end) {
    const int len = end - begin;
    const int rand_offset = (len == 0) ? 0 : (NextRandomInt() % (end - begin));
    return begin + rand_offset;
  }

  std::unique_ptr<AudioPushFifo> fifo_;
  std::vector<OutputChunkResult> results_;

 private:
  // Called by |fifo_| to deliver another chunk of audio.  Sanity checks
  // the sample values are as expected, and without any dropped/duplicated, and
  // adds a result to |results_|.
  void ReceiveAndCheckNextChunk(const AudioBus& audio_bus, int frame_delay) {
    OutputChunkResult result;
    result.num_frames = audio_bus.frames();
    result.first_sample_value = audio_bus.channel(0)[0];
    result.last_sample_value = audio_bus.channel(0)[audio_bus.frames() - 1];
    result.frame_delay = frame_delay;

    // Check that each sample value is the previous sample value plus one.
    for (int i = 1; i < audio_bus.frames(); ++i) {
      const float expected_value = result.first_sample_value + i;
      const float actual_value = audio_bus.channel(0)[i];
      if (actual_value != expected_value) {
        if (actual_value == 0.0f) {
          // This chunk is probably being emitted by a Flush().  If that's true
          // then the frame_delay will be negative and the rest of the
          // |audio_bus| should be all zeroes.
          ASSERT_GT(0, frame_delay);
          for (int j = i + 1; j < audio_bus.frames(); ++j)
            ASSERT_EQ(0.0f, audio_bus.channel(0)[j]);
          break;
        } else {
          ASSERT_EQ(expected_value, actual_value) << "Sample at offset " << i
                                                  << " is incorrect.";
        }
      }
    }

    results_.push_back(result);
  }

  // Note: Not using base::RandInt() because it is horribly slow on debug
  // builds.  The following is a very simple, deterministic LCG:
  int NextRandomInt() {
    rand_seed_ = (1103515245 * rand_seed_ + 12345) % (1 << 31);
    return static_cast<int>(rand_seed_);
  }

  uint32_t rand_seed_ = 0x7e110;
};

// Tests an atypical edge case: Push()ing one frame at a time.
TEST_P(AudioPushFifoTest, PushOneFrameAtATime) {
  RunSimpleRechunkTest(1);
}

// Tests that re-chunking the audio from common platform input chunk sizes
// works.
TEST_P(AudioPushFifoTest, Push128FramesAtATime) {
  RunSimpleRechunkTest(128);
}
TEST_P(AudioPushFifoTest, Push512FramesAtATime) {
  RunSimpleRechunkTest(512);
}

// Tests that re-chunking the audio from common "10 ms" input chunk sizes
// works (44100 Hz * 10 ms = 441, and 48000 Hz * 10 ms = 480).
TEST_P(AudioPushFifoTest, Push441FramesAtATime) {
  RunSimpleRechunkTest(441);
}
TEST_P(AudioPushFifoTest, Push480FramesAtATime) {
  RunSimpleRechunkTest(480);
}

// Tests that re-chunking when input audio is provided in varying chunk sizes
// works.
TEST_P(AudioPushFifoTest, PushArbitraryNumbersOfFramesAtATime) {
  // The loop below will run until both: 1) kMinNumIterations loops have
  // occurred; and 2) there are at least 3 entries in |results_|.
  const int kMinNumIterations = 30;

  int sample_value = 0;
  int frames_pushed_so_far = 0;
  for (int i = 0; i < kMinNumIterations || results_.size() < 3; ++i) {
    EXPECT_EQ(GetExpectedOutputChunks(frames_pushed_so_far), results_.size());

    // Create an AudioBus of a random length, populated with sample values.
    const int input_chunk_size = GetRandomInRange(1, 1920);
    const std::unique_ptr<AudioBus> audio_bus =
        AudioBus::Create(1, input_chunk_size);
    for (int j = 0; j < audio_bus->frames(); ++j)
      audio_bus->channel(0)[j] = static_cast<float>(sample_value++);

    fifo_->Push(*audio_bus);
    // Note: AudioPushFifo has just called ReceiveAndCheckNextChunk() zero or
    // more times.

    frames_pushed_so_far += input_chunk_size;
  }
  EXPECT_EQ(GetExpectedOutputChunks(frames_pushed_so_far), results_.size());

  ASSERT_FALSE(results_.empty());
  EXPECT_EQ(0.0f, results_.front().first_sample_value);
  const float last_value_in_last_chunk = static_cast<float>(
      GetExpectedOutputChunks(frames_pushed_so_far) * output_chunk_size() - 1);
  EXPECT_EQ(last_value_in_last_chunk, results_.back().last_sample_value);

  const size_t num_results_before_flush = results_.size();
  fifo_->Flush();
  const size_t num_results_after_flush = results_.size();
  if (num_results_after_flush > num_results_before_flush) {
    EXPECT_NE(0, results_.back().frame_delay);
    EXPECT_LT(-output_chunk_size(), results_.back().frame_delay);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AudioPushFifoTest,
                         ::testing::Values(
                             // 1 ms output chunks at common sample rates.
                             16,  // 16000 Hz
                             22,  // 22050 Hz
                             44,  // 44100 Hz
                             48,  // 48000 Hz

                             // 10 ms output chunks at common sample rates.
                             160,  // 16000 Hz
                             220,  // 22050 Hz
                             441,  // 44100 Hz
                             480,  // 48000 Hz

                             // 60 ms output chunks at common sample rates.
                             960,   // 16000 Hz
                             1323,  // 22050 Hz
                             2646,  // 44100 Hz
                             2880   // 48000 Hz
                             ));

}  // namespace

}  // namespace media
