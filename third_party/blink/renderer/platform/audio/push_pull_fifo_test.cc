// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/push_pull_fifo.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// Check the basic contract of FIFO. This test only covers the single thread
// scenario.
TEST(PushPullFIFOBasicTest, BasicTests) {
  // This suppresses the multi-thread warning for GTest. Potently it increases
  // the test execution time, but this specific test is very short and simple.
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  const unsigned kRenderQuantumFrames = 128;

  std::unique_ptr<PushPullFIFO> test_fifo =
      std::make_unique<PushPullFIFO>(2, 1024);

  // The input bus length must be |audio_utilities::kRenderQuantumFrames|.
  // i.e.) input_bus->length() == kRenderQuantumFrames
  scoped_refptr<AudioBus> input_bus_129_frames =
      AudioBus::Create(2, kRenderQuantumFrames + 1);
  EXPECT_DEATH_IF_SUPPORTED(test_fifo->Push(input_bus_129_frames.get()), "");
  scoped_refptr<AudioBus> input_bus_127_frames =
      AudioBus::Create(2, kRenderQuantumFrames - 1);
  EXPECT_DEATH_IF_SUPPORTED(test_fifo->Push(input_bus_127_frames.get()), "");

  // Pull request frames cannot exceed the length of output bus.
  // i.e.) frames_requested <= output_bus->length()
  scoped_refptr<AudioBus> output_bus_512_frames = AudioBus::Create(2, 512);
  EXPECT_DEATH_IF_SUPPORTED(test_fifo->Pull(output_bus_512_frames.get(), 513),
                            "");

  // Pull request frames cannot exceed the length of FIFO.
  // i.e.) frames_requested <= fifo_length_
  scoped_refptr<AudioBus> output_bus_1025_frames = AudioBus::Create(2, 1025);
  EXPECT_DEATH_IF_SUPPORTED(test_fifo->Pull(output_bus_1025_frames.get(), 1025),
                            "");
}

// Fills each AudioChannel in an AudioBus with a series of linearly increasing
// values starting from |starting_value| and incrementing by 1. Then return
// value will be |starting_value| + |bus_length|.
size_t FillBusWithLinearRamp(AudioBus* target_bus, size_t starting_value) {
  for (unsigned c = 0; c < target_bus->NumberOfChannels(); ++c) {
    float* bus_channel = target_bus->Channel(c)->MutableData();
    for (size_t i = 0; i < target_bus->Channel(c)->length(); ++i) {
      bus_channel[i] = static_cast<float>(starting_value + i);
    }
  }
  return starting_value + target_bus->length();
}

// Inspect the content of AudioBus with a given set of index and value across
// channels.
bool VerifyBusValueAtIndex(AudioBus* target_bus,
                           int index,
                           float expected_value) {
  for (unsigned c = 0; c < target_bus->NumberOfChannels(); ++c) {
    float* bus_channel = target_bus->Channel(c)->MutableData();
    if (bus_channel[index] != expected_value) {
      LOG(ERROR) << ">> [FAIL] expected " << expected_value << " at index "
                 << index << " but got " << bus_channel[index] << ".";
      return false;
    }
  }
  return true;
}

struct FIFOAction {
  // The type of action; "PUSH" or "PULL".
  const char* action;
  // Number of frames for the operation.
  const size_t number_of_frames;
};

struct AudioBusSample {
  // The frame index of a sample in the bus.
  const size_t index;
  // The value at the |index| above.
  const float value;
};

struct FIFOTestSetup {
  // Length of FIFO to be created for test case.
  const size_t fifo_length;
  // Channel count of FIFO to be created for test case.
  const unsigned number_of_channels;
  // A list of |FIFOAction| entries to be performed in test case.
  const std::vector<FIFOAction> fifo_actions;
};

struct FIFOTestExpectedState {
  // Expected read index in FIFO.
  const size_t index_read;
  // Expected write index in FIFO.
  const size_t index_write;
  // Expected overflow count in FIFO.
  const unsigned overflow_count;
  // Expected underflow count in FIFO.
  const unsigned underflow_count;
  // A list of expected |AudioBusSample| entries for the FIFO bus.
  const std::vector<AudioBusSample> fifo_samples;
  // A list of expected |AudioBusSample| entries for the output bus.
  const std::vector<AudioBusSample> output_samples;
};

// The data structure for the parameterized test cases.
struct FIFOTestParam {
  FIFOTestSetup setup;
  FIFOTestExpectedState expected_state;
};

std::ostream& operator<<(std::ostream& out, const FIFOTestParam& param) {
  out << "fifoLength=" << param.setup.fifo_length
      << " numberOfChannels=" << param.setup.number_of_channels;
  return out;
}

class PushPullFIFOFeatureTest : public testing::TestWithParam<FIFOTestParam> {};

TEST_P(PushPullFIFOFeatureTest, FeatureTests) {
  const FIFOTestSetup setup = GetParam().setup;
  const FIFOTestExpectedState expected_state = GetParam().expected_state;

  // Create a FIFO with a specified configuration.
  std::unique_ptr<PushPullFIFO> fifo = std::make_unique<PushPullFIFO>(
      setup.number_of_channels, setup.fifo_length);

  scoped_refptr<AudioBus> output_bus;

  // Iterate all the scheduled push/pull actions.
  size_t frame_counter = 0;
  for (const auto& action : setup.fifo_actions) {
    if (strcmp(action.action, "PUSH") == 0) {
      scoped_refptr<AudioBus> input_bus =
          AudioBus::Create(setup.number_of_channels, action.number_of_frames);
      frame_counter = FillBusWithLinearRamp(input_bus.get(), frame_counter);
      fifo->Push(input_bus.get());
      LOG(INFO) << "PUSH " << action.number_of_frames
                << " frames (frameCounter=" << frame_counter << ")";
    } else {
      output_bus =
          AudioBus::Create(setup.number_of_channels, action.number_of_frames);
      fifo->Pull(output_bus.get(), action.number_of_frames);
      LOG(INFO) << "PULL " << action.number_of_frames << " frames";
    }
  }

  // Get FIFO config data.
  const PushPullFIFOStateForTest actual_state = fifo->GetStateForTest();

  // Verify the read/write indexes.
  EXPECT_EQ(expected_state.index_read, actual_state.index_read);
  EXPECT_EQ(expected_state.index_write, actual_state.index_write);
  EXPECT_EQ(expected_state.overflow_count, actual_state.overflow_count);
  EXPECT_EQ(expected_state.underflow_count, actual_state.underflow_count);

  // Verify in-FIFO samples.
  for (const auto& sample : expected_state.fifo_samples) {
    EXPECT_TRUE(VerifyBusValueAtIndex(fifo->GetFIFOBusForTest(),
                                      sample.index, sample.value));
  }

  // Verify samples from the most recent output bus.
  for (const auto& sample : expected_state.output_samples) {
    EXPECT_TRUE(
        VerifyBusValueAtIndex(output_bus.get(), sample.index, sample.value));
  }
}

FIFOTestParam g_feature_test_params[] = {
    // Test cases 0 ~ 3: Regular operation on various channel configuration.
    //  - Mono, Stereo, Quad, 5.1.
    //  - FIFO length and pull size are RQ-aligned.
    {{512, 1, {{"PUSH", 128}, {"PUSH", 128}, {"PULL", 256}}},
     {256, 256, 0, 0, {{0, 0}}, {{0, 0}, {255, 255}}}},

    {{512, 2, {{"PUSH", 128}, {"PUSH", 128}, {"PULL", 256}}},
     {256, 256, 0, 0, {{0, 0}}, {{0, 0}, {255, 255}}}},

    {{512, 4, {{"PUSH", 128}, {"PUSH", 128}, {"PULL", 256}}},
     {256, 256, 0, 0, {{0, 0}}, {{0, 0}, {255, 255}}}},

    {{512, 6, {{"PUSH", 128}, {"PUSH", 128}, {"PULL", 256}}},
     {256, 256, 0, 0, {{0, 0}}, {{0, 0}, {255, 255}}}},

    // Test case 4: Pull size less than or equal to 128.
    {{128, 2, {{"PUSH", 128}, {"PULL", 128}, {"PUSH", 128}, {"PULL", 64}}},
     {64, 0, 0, 0, {{64, 192}, {0, 128}}, {{0, 128}, {63, 191}}}},

    // Test case 5: Unusual FIFO and Pull length.
    //  - FIFO and pull length that are not aligned to render quantum.
    //  - Check if the indexes are wrapping around correctly.
    //  - Check if the output bus starts and ends with correct values.
    {{997,
      1,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 449},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 449},
      }},
     // - expectedIndexRead = 898, expectedIndexWrite = 27
     // - overflowCount = 0, underflowCount = 0
     // - FIFO samples (index, expectedValue) = (898, 898), (27, 27)
     // - Output bus samples (index, expectedValue) = (0, 499), (448, 897)
     {898, 27, 0, 0, {{898, 898}, {27, 27}}, {{0, 449}, {448, 897}}}},

    // Test case 6: Overflow
    //  - Check overflow counter.
    //  - After the overflow occurs, the read index must be moved to the write
    //    index. Thus pulled frames must not contain overwritten data.
    {{512,
      3,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 256},
      }},
     // - expectedIndexRead = 384, expectedIndexWrite = 128
     // - overflowCount = 1, underflowCount = 0
     // - FIFO samples (index, expectedValue) = (384, 384), (128, 128)
     // - Output bus samples (index, expectedValue) = (0, 128), (255, 383)
     {384, 128, 1, 0, {{384, 384}, {128, 128}}, {{0, 128}, {255, 383}}}},

    // Test case 7: Overflow in unusual FIFO and pull length.
    //  - Check overflow counter.
    //  - After the overflow occurs, the read index must be moved to the write
    //    index. Thus pulled frames must not contain overwritten data.
    {{577,
      5,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 227},
      }},
     // - expectedIndexRead = 290, expectedIndexWrite = 63
     // - overflowCount = 1, underflowCount = 0
     // - FIFO samples (index, expectedValue) = (63, 63), (290, 290)
     // - Output bus samples (index, expectedValue) = (0, 63), (226, 289)
     {290, 63, 1, 0, {{63, 63}, {290, 290}}, {{0, 63}, {226, 289}}}},

    // Test case 8: Underflow
    //  - Check underflow counter.
    //  - After the underflow occurs, the write index must be moved to the read
    //    index. Frames pulled after FIFO underflows must be zeroed.
    {{512,
      7,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 384},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 384},
      }},
     // - expectedIndexRead = 128, expectedIndexWrite = 128
     // - overflowCount = 0, underflowCount = 1
     // - FIFO samples (index, expectedValue) = (128, 128)
     // - Output bus samples (index, expectedValue) = (0, 384), (255, 639)
     //                                               (256, 0), (383, 0)
     {128,
      128,
      0,
      1,
      {{128, 128}},
      {{0, 384}, {255, 639}, {256, 0}, {383, 0}}}},

    // Test case 9: Underflow in unusual FIFO and pull length.
    //  - Check underflow counter.
    //  - After the underflow occurs, the write index must be moved to the read
    //    index. Frames pulled after FIFO underflows must be zeroed.
    {{523,
      11,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 383},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 383},
      }},
     // - expectedIndexRead = 117, expectedIndexWrite = 117
     // - overflowCount = 0, underflowCount = 1
     // - FIFO samples (index, expectedValue) = (117, 117)
     // - Output bus samples (index, expectedValue) = (0, 383), (256, 639)
     //                                               (257, 0), (382, 0)
     {117,
      117,
      0,
      1,
      {{117, 117}},
      {{0, 383}, {256, 639}, {257, 0}, {382, 0}}}},

    // Test case 10: Multiple pull from an empty FIFO.
    //  - Check underflow counter.
    //  - After the underflow occurs, the write index must be moved to the read
    //    index. Frames pulled after FIFO underflows must be zeroed.
    {{1024,
      11,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 440},
          {"PULL", 440},
          {"PULL", 440},
          {"PULL", 440},
          {"PULL", 440},
      }},
     // - expectedIndexRead = 117, expectedIndexWrite = 117
     // - overflowCount = 0, underflowCount = 1
     // - FIFO samples (index, expectedValue) = (117, 117)
     // - Output bus samples (index, expectedValue) = (0, 383), (256, 639)
     //                                               (257, 0), (382, 0)
     {256, 256, 0, 5, {{256, 0}}, {{0, 0}, {439, 0}}}},

    // Test case 11: Multiple pull from an empty FIFO. (zero push)
    {{1024,
      11,
      {
          {"PULL", 144},
          {"PULL", 144},
          {"PULL", 144},
          {"PULL", 144},
      }},
     // - expectedIndexRead = 0, expectedIndexWrite = 0
     // - overflowCount = 0, underflowCount = 4
     // - FIFO samples (index, expectedValue) = (0, 0), (1023, 0)
     // - Output bus samples (index, expectedValue) = (0, 0), (143, 0)
     {0, 0, 0, 4, {{0, 0}, {1023, 0}}, {{0, 0}, {143, 0}}}}};

INSTANTIATE_TEST_SUITE_P(PushPullFIFOFeatureTest,
                         PushPullFIFOFeatureTest,
                         testing::ValuesIn(g_feature_test_params));


struct FIFOEarmarkTestParam {
  FIFOTestSetup setup;
  size_t callback_buffer_size;
  size_t expected_earmark_frames;
};

class PushPullFIFOEarmarkFramesTest
    : public testing::TestWithParam<FIFOEarmarkTestParam> {};

TEST_P(PushPullFIFOEarmarkFramesTest, FeatureTests) {
  const FIFOTestSetup setup = GetParam().setup;
  const size_t callback_buffer_size = GetParam().callback_buffer_size;
  const size_t expected_earmark_frames = GetParam().expected_earmark_frames;

  // Create a FIFO with a specified configuration.
  std::unique_ptr<PushPullFIFO> fifo = std::make_unique<PushPullFIFO>(
      setup.number_of_channels, setup.fifo_length);
  fifo->SetEarmarkFrames(callback_buffer_size);

  scoped_refptr<AudioBus> output_bus;

  // Iterate all the scheduled push/pull actions.
  size_t frame_counter = 0;
  for (const auto& action : setup.fifo_actions) {
    if (strcmp(action.action, "PUSH") == 0) {
      scoped_refptr<AudioBus> input_bus =
          AudioBus::Create(setup.number_of_channels, action.number_of_frames);
      frame_counter = FillBusWithLinearRamp(input_bus.get(), frame_counter);
      fifo->Push(input_bus.get());
      LOG(INFO) << "PUSH " << action.number_of_frames
                << " frames (frameCounter=" << frame_counter << ")";
    } else if (strcmp(action.action, "PULL_EARMARK") == 0) {
      output_bus =
          AudioBus::Create(setup.number_of_channels, action.number_of_frames);
      fifo->PullAndUpdateEarmark(output_bus.get(), action.number_of_frames);
      LOG(INFO) << "PULL_EARMARK " << action.number_of_frames << " frames";
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  // Test the earmark frames.
  const size_t actual_earmark_frames = fifo->GetEarmarkFramesForTest();
  EXPECT_EQ(expected_earmark_frames, actual_earmark_frames);
}

FIFOEarmarkTestParam g_earmark_test_params[] = {
  // When there's no underrun, the earmark is equal to the callback size.
  {{8192, 2, {
      {"PUSH", 128},
      {"PUSH", 128},
      {"PULL_EARMARK", 256},
      {"PUSH", 128},
      {"PUSH", 128},
      {"PULL_EARMARK", 256}
    }}, 256, 256},
  // The first underrun increases the earmark by the callback size.
  {{8192, 2, {
      {"PUSH", 128},
      {"PUSH", 128},
      {"PULL_EARMARK", 384}, // udnerrun; updating earmark and skipping pull.
      {"PUSH", 128},
      {"PUSH", 128},
      {"PUSH", 128},
      {"PULL_EARMARK", 384}  // OK
    }}, 384, 768},
  // Simulating "bursty and irregular" callbacks.
  {{8192, 2, {
      {"PUSH", 128},
      {"PUSH", 128},
      {"PUSH", 128},
      {"PUSH", 128},
      {"PULL_EARMARK", 480}, // OK
      {"PUSH", 128},
      {"PUSH", 128},
      {"PULL_EARMARK", 480}, // underrun; updating earmark and skipping pull.
      {"PUSH", 128},
      {"PUSH", 128},
      {"PUSH", 128},
      {"PULL_EARMARK", 480}, // OK
      {"PUSH", 128},
      {"PULL_EARMARK", 480}  // underrun; updating earmark and skipping pull.
    }}, 480, 1440}
};

INSTANTIATE_TEST_SUITE_P(PushPullFIFOEarmarkFramesTest,
                         PushPullFIFOEarmarkFramesTest,
                         testing::ValuesIn(g_earmark_test_params));

}  // namespace

}  // namespace blink
