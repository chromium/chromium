// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_bus.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <limits>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using AlignedFloatArray = base::AlignedHeapArray<float>;

static const int kChannels = 6;
static constexpr ChannelLayout kChannelLayout = CHANNEL_LAYOUT_5_1;
// Use a buffer size which is intentionally not a multiple of kChannelAlignment.
static const size_t kFrameCount = media::AudioBus::kChannelAlignment * 32 - 1;
static const int kSampleRate = 48000;

class AudioBusTest : public testing::Test {
 public:
  AudioBusTest() = default;

  AudioBusTest(const AudioBusTest&) = delete;
  AudioBusTest& operator=(const AudioBusTest&) = delete;

  ~AudioBusTest() override = default;

  void VerifyChannelAndFrameCount(AudioBus* bus) {
    EXPECT_EQ(kChannels, bus->channels());
    EXPECT_EQ(static_cast<int>(kFrameCount), bus->frames());
  }

  void VerifySpanIsFilledWithValue(AudioBus::ConstChannel data, float value) {
    for (size_t i = 0; i < data.size(); ++i) {
      ASSERT_FLOAT_EQ(value, data[i]) << "i=" << i;
    }
  }

  // Verify values for each channel in |result| are within |epsilon| of
  // |expected|.  If |epsilon| exactly equals 0, uses FLOAT_EQ macro.
  void VerifyAreEqualWithEpsilon(const AudioBus* result,
                                 const AudioBus* expected,
                                 float epsilon) {
    ASSERT_EQ(expected->channels(), result->channels());
    ASSERT_EQ(expected->frames(), result->frames());
    ASSERT_EQ(expected->is_bitstream_format(), result->is_bitstream_format());

    if (expected->is_bitstream_format()) {
      ASSERT_EQ(expected->GetBitstreamFrames(), result->GetBitstreamFrames());
      ASSERT_EQ(expected->bitstream_data(), result->bitstream_data());
      return;
    }

    for (int ch = 0; ch < result->channels(); ++ch) {
      for (int i = 0; i < result->frames(); ++i) {
        SCOPED_TRACE(base::StringPrintf("ch=%d, i=%d", ch, i));

        if (epsilon == 0) {
          ASSERT_FLOAT_EQ(expected->channel_span(ch)[i],
                          result->channel_span(ch)[i]);
        } else {
          ASSERT_NEAR(expected->channel_span(ch)[i],
                      result->channel_span(ch)[i], epsilon);
        }
      }
    }
  }

  // Verify values for each channel in |result| against |expected|.
  void VerifyAreEqual(const AudioBus* result, const AudioBus* expected) {
    VerifyAreEqualWithEpsilon(result, expected, 0);
  }

  // Read and write to the full extent of the allocated channel data.  Also test
  // the Zero() method and verify it does as advertised.  Also test data if data
  // is 16-byte aligned as advertised (see kChannelAlignment in audio_bus.h).
  void VerifyReadWriteAndAlignment(AudioBus* bus) {
    int channel_count = 0;
    for (auto channel : bus->AllChannels()) {
      // Verify that the address returned by channel(i) is a multiple of
      // AudioBus::kChannelAlignment.
      ASSERT_TRUE(AudioBus::IsAligned(channel));
      // Write into the channel buffer.
      std::ranges::fill(channel, channel_count++);
    }

    for (int i = 0; i < bus->channels(); ++i) {
      VerifySpanIsFilledWithValue(bus->channel_span(i), i);
    }

    bus->Zero();
    for (int i = 0; i < bus->channels(); ++i) {
      VerifySpanIsFilledWithValue(bus->channel_span(i), 0);
    }
  }

  // Verify copying to and from |bus1| and |bus2|.
  void CopyTest(AudioBus* bus1, AudioBus* bus2) {
    // Fill |bus1| with dummy data.
    if (bus1->is_bitstream_format()) {
      uint8_t filler = 1;
      std::ranges::generate(bus1->bitstream_data(),
                            [filler]() mutable { return filler++; });
    } else {
      int filler = 1;
      for (auto channel : bus1->AllChannels()) {
        std::ranges::fill(channel, filler);
      }
    }

    // Verify copy from |bus1| to |bus2|.
    bus2->Zero();
    bus1->CopyTo(bus2);
    VerifyAreEqual(bus1, bus2);

    // Verify copy from |bus2| to |bus1|.
    bus1->Zero();
    bus2->CopyTo(bus1);
    VerifyAreEqual(bus2, bus1);
  }

 protected:
  void AllocateDataPerChannel() {
    data_.reserve(kChannels);
    for (int i = 0; i < kChannels; ++i) {
      data_.push_back(
          base::AlignedUninit<float>(kFrameCount, AudioBus::kChannelAlignment));
    }
  }

  std::vector<AlignedFloatArray> data_;
};

// Verify basic Create(...) method works as advertised.
TEST_F(AudioBusTest, Create) {
  std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);
  VerifyChannelAndFrameCount(bus.get());
  VerifyReadWriteAndAlignment(bus.get());
}

// Verify Create(...) using AudioParameters works as advertised.
TEST_F(AudioBusTest, CreateUsingAudioParameters) {
  std::unique_ptr<AudioBus> bus = AudioBus::Create(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::FromLayout<kChannelLayout>(),
                      kSampleRate, kFrameCount));
  VerifyChannelAndFrameCount(bus.get());
  VerifyReadWriteAndAlignment(bus.get());
}

// Verify an AudioBus created via CreateWrapper(...) works as advertised.
TEST_F(AudioBusTest, CreateWrapper) {
  AllocateDataPerChannel();

  std::unique_ptr<AudioBus> bus = AudioBus::CreateWrapper(kChannels);
  bus->set_frames(kFrameCount);
  for (int i = 0; i < bus->channels(); ++i) {
    bus->SetChannelData(i, data_[i].as_span());
  }

  bool deleted = false;
  bus->SetWrappedDataDeleter(
      base::BindLambdaForTesting([&]() { deleted = true; }));

  VerifyChannelAndFrameCount(bus.get());
  VerifyReadWriteAndAlignment(bus.get());

  EXPECT_FALSE(deleted);
  bus.reset();
  EXPECT_TRUE(deleted);
}

TEST_F(AudioBusTest, AllChannels) {
  AllocateDataPerChannel();

  std::unique_ptr<AudioBus> bus = AudioBus::CreateWrapper(kChannels);
  bus->set_frames(kFrameCount);
  AudioBus::ChannelVector channels;
  int value = 1;
  for (AlignedFloatArray& data : data_) {
    AudioBus::Channel channel(data);

    // Fill each channel with a different value.
    std::ranges::fill(channel, value++);
    channels.push_back(channel);
  }

  bus->SetAllChannels(channels);

  VerifyChannelAndFrameCount(bus.get());

  // Verify looping through `AllChannels()` is equivalent to getting each
  // channel individually.
  int current_channel = 0;
  for (auto channel : bus->AllChannels()) {
    EXPECT_EQ(channel, bus->channel_span(current_channel++));
  }

  EXPECT_EQ(current_channel, kChannels);
}

TEST_F(AudioBusTest, AllChannelsSubspan) {
  AllocateDataPerChannel();

  std::unique_ptr<AudioBus> bus = AudioBus::CreateWrapper(kChannels);
  bus->set_frames(kFrameCount);
  AudioBus::ChannelVector channels;
  int value = 1;
  for (AlignedFloatArray& data : data_) {
    AudioBus::Channel channel(data);

    // Fill each sample with a different value.
    for (float& sample : channel) {
      sample = value++;
    }

    channels.push_back(channel);
  }

  bus->SetAllChannels(channels);

  // Verify looping through `AllChannelsSubspan()` is equivalent to getting each
  // channel individually and applying `subspan()` to them.
  int current_channel = 0;
  constexpr size_t kOffset = 3;
  constexpr size_t kCount = 25;
  for (auto channel : bus->AllChannelsSubspan(kOffset, kCount)) {
    EXPECT_EQ(channel,
              bus->channel_span(current_channel++).subspan(kOffset, kCount));
  }

  EXPECT_EQ(current_channel, kChannels);
}

// Verify an AudioBus created via wrapping a memory block works as advertised.
TEST_F(AudioBusTest, WrapMemory) {
  auto verify_wrapped_memory = [&](bool use_byte_span) {
    AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                           ChannelLayoutConfig::FromLayout<kChannelLayout>(),
                           kSampleRate, kFrameCount);

    const size_t total_frame_count =
        AudioBus::CalculateMemorySize(params) / sizeof(float);
    auto float_data = base::AlignedUninit<float>(total_frame_count,
                                                 AudioBus::kChannelAlignment);
    base::span<float> float_span = base::span(float_data);

    // Fill the memory with a test value we can check for after wrapping.
    static constexpr float kTestValue = 3;
    std::ranges::fill(float_data, kTestValue);

    std::unique_ptr<AudioBus> bus;

    if (use_byte_span) {
      base::span<uint8_t> byte_span =
          base::as_writable_bytes(base::allow_nonunique_obj, float_span);
      bus = AudioBus::WrapMemory(params, byte_span);
    } else {
      bus = AudioBus::WrapMemory(params, float_span);
    }

    // Verify the test value we filled prior to wrapping.
    for (auto channel : bus->AllChannels()) {
      VerifySpanIsFilledWithValue(channel, kTestValue);
    }
    VerifyChannelAndFrameCount(bus.get());
    VerifyReadWriteAndAlignment(bus.get());

    auto all_channels = bus->AllChannels();
    auto first_channel = all_channels.front();
    auto last_channel = all_channels.back();

    // Verify the channel vectors lie within the provided memory block.
    EXPECT_GE(&first_channel.front(), &float_span.front());
    EXPECT_LT(&last_channel.back(), &float_span.back());
  };

  {
    SCOPED_TRACE("uint8_t span");
    verify_wrapped_memory(/*use_byte_span=*/true);
  }
  {
    SCOPED_TRACE("float span");
    verify_wrapped_memory(/*use_byte_span=*/false);
  }
}

// Simulate a shared memory transfer and verify results.
TEST_F(AudioBusTest, CopyTo) {
  // Create one bus with AudioParameters and the other through direct values to
  // test for parity between the Create() functions.
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::FromLayout<kChannelLayout>(),
                         kSampleRate, kFrameCount);
  std::unique_ptr<AudioBus> bus1 = AudioBus::Create(kChannels, kFrameCount);
  std::unique_ptr<AudioBus> bus2 = AudioBus::Create(params);

  const size_t memory_size = AudioBus::CalculateMemorySize(params);

  {
    SCOPED_TRACE("Created");
    CopyTest(bus1.get(), bus2.get());
  }
  {
    SCOPED_TRACE("Wrapped Memory - byte span");
    // Try a copy to an AudioBus wrapping a memory block.
    auto data =
        base::AlignedUninit<uint8_t>(memory_size, AudioBus::kChannelAlignment);

    bus2 = AudioBus::WrapMemory(params, data.as_span());
    CopyTest(bus1.get(), bus2.get());
  }
  {
    SCOPED_TRACE("Wrapped Memory - float span");
    // Try a copy to an AudioBus wrapping a memory block.
    auto data = base::AlignedUninit<float>(memory_size / sizeof(float),
                                           AudioBus::kChannelAlignment);

    bus2 = AudioBus::WrapMemory(params, data.as_span());
    CopyTest(bus1.get(), bus2.get());
  }
}

// Verify Zero() and ZeroFrames(...) utility methods work as advertised.
TEST_F(AudioBusTest, Zero) {
  std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);

  // Fill the bus with dummy data.
  int value = 1;
  for (auto channel : bus->AllChannels()) {
    std::ranges::fill(channel, value++);
  }
  EXPECT_FALSE(bus->AreFramesZero());

  // Zero first half the frames of each channel.
  bus->ZeroFrames(kFrameCount / 2);

  value = 1;
  for (auto channel : bus->AllChannels()) {
    SCOPED_TRACE("First Half Zero");
    auto [first_half, second_half] = channel.split_at(kFrameCount / 2);
    VerifySpanIsFilledWithValue(first_half, 0);
    VerifySpanIsFilledWithValue(second_half, value++);
  }
  EXPECT_FALSE(bus->AreFramesZero());

  // Fill the bus with dummy data.
  value = 1;
  for (auto channel : bus->AllChannels()) {
    std::ranges::fill(channel, value++);
  }

  // Zero the last half of the frames.
  static constexpr size_t kRemainingFrames = kFrameCount - kFrameCount / 2;
  bus->ZeroFramesPartial(kFrameCount / 2, kRemainingFrames);
  value = 1;
  for (auto channel : bus->AllChannels()) {
    SCOPED_TRACE("Last Half Zero");
    auto [first_half, second_half] = channel.split_at(kFrameCount / 2);
    VerifySpanIsFilledWithValue(first_half, value++);
    VerifySpanIsFilledWithValue(second_half, 0);
  }
  EXPECT_FALSE(bus->AreFramesZero());

  // Fill the bus with dummy data.
  value = 1;
  for (auto channel : bus->AllChannels()) {
    std::ranges::fill(channel, value++);
  }

  // Zero all the frames of each channel.
  bus->Zero();
  for (auto channel : bus->AllChannels()) {
    SCOPED_TRACE("All Zero");
    VerifySpanIsFilledWithValue(channel, 0);
  }
  EXPECT_TRUE(bus->AreFramesZero());
}

// Each test vector represents two channels of data in the following arbitrary
// layout: <min, zero, max, min, max / 2, min / 2, zero, max, zero, zero>.
static constexpr int kTestVectorSize = 10;
static const uint8_t kTestVectorUint8[kTestVectorSize] = {
    0,         -INT8_MIN,          UINT8_MAX,
    0,         INT8_MAX / 2 + 128, INT8_MIN / 2 + 128,
    -INT8_MIN, UINT8_MAX,          -INT8_MIN,
    -INT8_MIN};
static const int16_t kTestVectorInt16[kTestVectorSize] = {
    INT16_MIN,     0, INT16_MAX, INT16_MIN, INT16_MAX / 2,
    INT16_MIN / 2, 0, INT16_MAX, 0,         0};
static const int32_t kTestVectorInt32[kTestVectorSize] = {
    INT32_MIN,     0, INT32_MAX, INT32_MIN, INT32_MAX / 2,
    INT32_MIN / 2, 0, INT32_MAX, 0,         0};
static const float kTestVectorFloat32[kTestVectorSize] = {
    -1.0f, 0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f};

// This is based on kTestVectorFloat32, but has some of the values outside of
// sanity.
static const float kTestVectorFloat32Invalid[kTestVectorSize] = {
    -5.0f,
    0.0f,
    5.0f,
    -1.0f,
    0.5f,
    -0.5f,
    0.0f,
    std::numeric_limits<float>::infinity(),
    std::numeric_limits<float>::signaling_NaN(),
    std::numeric_limits<float>::quiet_NaN()};

static const float kTestVectorFloat32Sanitized[kTestVectorSize] = {
    -1.0f, 0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f};

// Expected results.
static constexpr size_t kTestVectorFrameCount = kTestVectorSize / 2;
static const auto kTestVectorResult =
    std::to_array<std::array<const float, kTestVectorFrameCount>>(
        {{-1.0f, 1.0f, 0.5f, 0.0f, 0.0f}, {0.0f, -1.0f, -0.5f, 1.0f, 0.0f}});
static const int kTestVectorChannelCount = std::size(kTestVectorResult);

// Verify FromInterleaved() deinterleaves audio in supported formats correctly.
TEST_F(AudioBusTest, FromInterleaved) {
  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  std::unique_ptr<AudioBus> expected =
      AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  for (int ch = 0; ch < kTestVectorChannelCount; ++ch) {
    expected->channel_span(ch).copy_from(base::span(kTestVectorResult[ch]));
  }

  {
    SCOPED_TRACE("UnsignedInt8SampleTypeTraits");
    bus->Zero();
    bus->FromInterleaved<UnsignedInt8SampleTypeTraits>(kTestVectorUint8,
                                                       kTestVectorFrameCount);
    // Biased uint8_t calculations have poor precision, so the epsilon here is
    // slightly more permissive than int16_t and int32_t calculations.
    VerifyAreEqualWithEpsilon(bus.get(), expected.get(),
                              1.0f / (std::numeric_limits<uint8_t>::max() - 1));
  }
  {
    SCOPED_TRACE("SignedInt16SampleTypeTraits");
    bus->Zero();
    bus->FromInterleaved<SignedInt16SampleTypeTraits>(kTestVectorInt16,
                                                      kTestVectorFrameCount);
    VerifyAreEqualWithEpsilon(
        bus.get(), expected.get(),
        1.0f /
            (static_cast<float>(std::numeric_limits<uint16_t>::max()) + 1.0f));
  }
  {
    SCOPED_TRACE("SignedInt32SampleTypeTraits");
    bus->Zero();
    bus->FromInterleaved<SignedInt32SampleTypeTraits>(kTestVectorInt32,
                                                      kTestVectorFrameCount);
    VerifyAreEqualWithEpsilon(
        bus.get(), expected.get(),
        1.0f / static_cast<float>(std::numeric_limits<uint32_t>::max()));
  }
  {
    SCOPED_TRACE("Float32SampleTypeTraits");
    bus->Zero();
    bus->FromInterleaved<Float32SampleTypeTraits>(kTestVectorFloat32,
                                                  kTestVectorFrameCount);
    VerifyAreEqual(bus.get(), expected.get());
  }
}

// Verify FromInterleavedPartial() deinterleaves audio correctly.
TEST_F(AudioBusTest, FromInterleavedPartial) {
  // Only deinterleave the middle two frames in each channel.
  static constexpr size_t kPartialStart = 1;
  static constexpr size_t kPartialFrames = 2;
  ASSERT_LE(kPartialStart + kPartialFrames, kTestVectorFrameCount);

  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  std::unique_ptr<AudioBus> expected =
      AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  expected->Zero();
  int current_channel = 0;
  for (auto partial_channel :
       expected->AllChannelsSubspan(kPartialStart, kPartialFrames)) {
    partial_channel.copy_from(base::span(kTestVectorResult[current_channel++])
                                  .subspan(kPartialStart, kPartialFrames));
  }

  {
    SCOPED_TRACE("SignedInt32SampleTypeTraits");
    bus->Zero();
    bus->FromInterleavedPartial<SignedInt32SampleTypeTraits>(
        UNSAFE_TODO(kTestVectorInt32 + kPartialStart * bus->channels()),
        kPartialStart, kPartialFrames);
    VerifyAreEqual(bus.get(), expected.get());
  }
}

// Verify ToInterleaved() interleaves audio in supported formats correctly.
TEST_F(AudioBusTest, ToInterleaved) {
  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  // Fill the bus with our test vector.
  for (int ch = 0; ch < bus->channels(); ++ch) {
    bus->channel_span(ch).copy_from(base::span(kTestVectorResult[ch]));
  }

  {
    SCOPED_TRACE("UnsignedInt8SampleTypeTraits");
    uint8_t test_array[std::size(kTestVectorUint8)];
    bus->ToInterleaved<UnsignedInt8SampleTypeTraits>(bus->frames(), test_array);
    UNSAFE_TODO(ASSERT_EQ(
        0, memcmp(test_array, kTestVectorUint8, sizeof(kTestVectorUint8))));
  }
  {
    SCOPED_TRACE("SignedInt16SampleTypeTraits");
    int16_t test_array[std::size(kTestVectorInt16)];
    bus->ToInterleaved<SignedInt16SampleTypeTraits>(bus->frames(), test_array);
    UNSAFE_TODO(ASSERT_EQ(
        0, memcmp(test_array, kTestVectorInt16, sizeof(kTestVectorInt16))));
  }
  {
    SCOPED_TRACE("SignedInt32SampleTypeTraits");
    int32_t test_array[std::size(kTestVectorInt32)];
    bus->ToInterleaved<SignedInt32SampleTypeTraits>(bus->frames(), test_array);

    // Some compilers get better precision than others on the half-max test, so
    // let the test pass with an off by one check on the half-max.
    int32_t alternative_acceptable_result[std::size(kTestVectorInt32)];
    UNSAFE_TODO(memcpy(alternative_acceptable_result, kTestVectorInt32,
                       sizeof(kTestVectorInt32)));
    ASSERT_EQ(alternative_acceptable_result[4],
              std::numeric_limits<int32_t>::max() / 2);
    alternative_acceptable_result[4]++;

    UNSAFE_TODO(ASSERT_TRUE(
        memcmp(test_array, kTestVectorInt32, sizeof(kTestVectorInt32)) == 0 ||
        memcmp(test_array, alternative_acceptable_result,
               sizeof(alternative_acceptable_result)) == 0));
  }
  {
    SCOPED_TRACE("Float32SampleTypeTraits");
    float test_array[std::size(kTestVectorFloat32)];
    bus->ToInterleaved<Float32SampleTypeTraits>(bus->frames(), test_array);
    UNSAFE_TODO(ASSERT_EQ(
        0, memcmp(test_array, kTestVectorFloat32, sizeof(kTestVectorFloat32))));
  }
}

TEST_F(AudioBusTest, ToInterleavedSanitized) {
  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  bus->FromInterleaved<Float32SampleTypeTraits>(kTestVectorFloat32Invalid,
                                                bus->frames());
  // Verify FromInterleaved applied no sanity.
  ASSERT_EQ(bus->channel_span(0)[0], kTestVectorFloat32Invalid[0]);
  std::array<float, std::size(kTestVectorFloat32Sanitized)> test_array;
  bus->ToInterleaved<Float32SampleTypeTraits>(bus->frames(), test_array.data());
  for (size_t i = 0; i < std::size(kTestVectorFloat32Sanitized); ++i)
    UNSAFE_TODO(ASSERT_EQ(kTestVectorFloat32Sanitized[i], test_array[i]));

  // Verify that Float32SampleTypeTraitsNoClip applied no sanity. Note: We don't
  // use memcmp() here since the NaN type may change on x86 platforms in certain
  // circumstances, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57484
  bus->ToInterleaved<Float32SampleTypeTraitsNoClip>(bus->frames(),
                                                    test_array.data());
  for (int i = 0; i < kTestVectorSize; ++i) {
    if (std::isnan(test_array[i]))
      UNSAFE_TODO(EXPECT_TRUE(std::isnan(kTestVectorFloat32Invalid[i])));
    else
      UNSAFE_TODO(EXPECT_FLOAT_EQ(test_array[i], kTestVectorFloat32Invalid[i]));
  }
}

TEST_F(AudioBusTest, CopyAndClipTo) {
  auto bus = AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  bus->FromInterleaved<Float32SampleTypeTraits>(kTestVectorFloat32Invalid,
                                                bus->frames());
  auto expected =
      AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  expected->FromInterleaved<Float32SampleTypeTraits>(
      kTestVectorFloat32Sanitized, bus->frames());

  // Verify FromInterleaved applied no sanity.
  ASSERT_EQ(bus->channel_span(0)[0], kTestVectorFloat32Invalid[0]);

  std::unique_ptr<AudioBus> copy_to_bus =
      AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  bus->CopyAndClipTo(copy_to_bus.get());

  for (int ch = 0; ch < expected->channels(); ++ch) {
    for (int i = 0; i < expected->frames(); ++i)
      ASSERT_EQ(copy_to_bus->channel_span(ch)[i],
                expected->channel_span(ch)[i]);
  }

  ASSERT_EQ(expected->channels(), copy_to_bus->channels());
  ASSERT_EQ(expected->frames(), copy_to_bus->frames());
  ASSERT_EQ(expected->is_bitstream_format(),
            copy_to_bus->is_bitstream_format());
}

// Verify ToInterleavedPartial() interleaves audio correctly.
TEST_F(AudioBusTest, ToInterleavedPartial) {
  // Only interleave the middle two frames in each channel.
  static constexpr size_t kPartialStart = 1;
  static constexpr size_t kPartialFrames = 2;
  ASSERT_LE(kPartialStart + kPartialFrames, kTestVectorFrameCount);

  std::unique_ptr<AudioBus> expected =
      AudioBus::Create(kTestVectorChannelCount, kTestVectorFrameCount);
  for (int ch = 0; ch < kTestVectorChannelCount; ++ch) {
    expected->channel_span(ch).copy_from(base::span(kTestVectorResult[ch]));
  }

  {
    SCOPED_TRACE("Float32SampleTypeTraits");
    float test_array[std::size(kTestVectorFloat32)];
    expected->ToInterleavedPartial<Float32SampleTypeTraits>(
        kPartialStart, kPartialFrames, test_array);
    UNSAFE_TODO(ASSERT_EQ(
        0, memcmp(test_array,
                  kTestVectorFloat32 + kPartialStart * kTestVectorChannelCount,
                  kPartialFrames * sizeof(*kTestVectorFloat32) *
                      kTestVectorChannelCount)));
  }
}

struct ZeroingOutTestData {
  static constexpr int kChannelCount = 2;
  static constexpr int kFrameCount = 10;
  static constexpr int kInterleavedFrameCount = 3;

  std::unique_ptr<AudioBus> bus_under_test;
  std::vector<float> interleaved_dummy_frames;

  ZeroingOutTestData() {
    // Create a bus and fill each channel with a test pattern of form
    // [1.0, 2.0, 3.0, ...]
    bus_under_test = AudioBus::Create(kChannelCount, kFrameCount);
    for (auto channel : bus_under_test->AllChannels()) {
      int value = 1;
      for (float& sample : channel) {
        sample = value++;
      }
    }

    // Create a vector containing dummy interleaved samples.
    static const float kDummySampleValue = 0.123f;
    interleaved_dummy_frames.resize(kChannelCount * kInterleavedFrameCount);
    std::fill(interleaved_dummy_frames.begin(), interleaved_dummy_frames.end(),
              kDummySampleValue);
  }
};

TEST_F(AudioBusTest, FromInterleavedZerosOutUntouchedFrames) {
  ZeroingOutTestData test_data;

  // Exercise
  test_data.bus_under_test->FromInterleaved<Float32SampleTypeTraits>(
      &test_data.interleaved_dummy_frames[0], test_data.kInterleavedFrameCount);

  const size_t untouched_frame_count =
      test_data.kFrameCount - test_data.kInterleavedFrameCount;

  // Verification
  for (auto partial_channel : test_data.bus_under_test->AllChannelsSubspan(
           test_data.kInterleavedFrameCount, untouched_frame_count)) {
    for (auto sample : partial_channel) {
      ASSERT_EQ(0.0f, sample);
    }
  }
}

TEST_F(AudioBusTest, FromInterleavedPartialDoesNotZeroOutUntouchedFrames) {
  {
    SCOPED_TRACE("Zero write offset");

    ZeroingOutTestData test_data;
    static const int kWriteOffsetInFrames = 0;

    // Exercise
    test_data.bus_under_test->FromInterleavedPartial<Float32SampleTypeTraits>(
        &test_data.interleaved_dummy_frames[0], kWriteOffsetInFrames,
        test_data.kInterleavedFrameCount);

    // Verification
    for (int ch = 0; ch < test_data.kChannelCount; ++ch) {
      auto sample_array_for_current_channel =
          test_data.bus_under_test->channel_span(ch);
      for (int frame_index =
               test_data.kInterleavedFrameCount + kWriteOffsetInFrames;
           frame_index < test_data.kFrameCount; frame_index++) {
        ASSERT_EQ(frame_index + 1,
                  sample_array_for_current_channel[frame_index]);
      }
    }
  }
  {
    SCOPED_TRACE("Positive write offset");

    ZeroingOutTestData test_data;
    static const int kWriteOffsetInFrames = 2;

    // Exercise
    test_data.bus_under_test->FromInterleavedPartial<Float32SampleTypeTraits>(
        &test_data.interleaved_dummy_frames[0], kWriteOffsetInFrames,
        test_data.kInterleavedFrameCount);

    // Verification
    for (int ch = 0; ch < test_data.kChannelCount; ++ch) {
      auto sample_array_for_current_channel =
          test_data.bus_under_test->channel_span(ch);
      // Check untouched frames before write offset
      for (int frame_index = 0; frame_index < kWriteOffsetInFrames;
           frame_index++) {
        ASSERT_EQ(frame_index + 1,
                  sample_array_for_current_channel[frame_index]);
      }
      // Check untouched frames after write
      for (int frame_index =
               test_data.kInterleavedFrameCount + kWriteOffsetInFrames;
           frame_index < test_data.kFrameCount; frame_index++) {
        ASSERT_EQ(frame_index + 1,
                  sample_array_for_current_channel[frame_index]);
      }
    }
  }
}

TEST_F(AudioBusTest, Scale) {
  std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);

  // Fill the bus with dummy data.
  static constexpr float kFillValue = 1;
  for (auto channel : bus->AllChannels()) {
    std::ranges::fill(channel, kFillValue);
  }

  // Adjust by an invalid volume and ensure volume is unchanged.
  bus->Scale(-1);
  for (auto channel : bus->AllChannels()) {
    SCOPED_TRACE("Invalid Scale");
    VerifySpanIsFilledWithValue(channel, kFillValue);
  }

  // Verify correct volume adjustment.
  static constexpr float kVolume = 0.5;
  bus->Scale(kVolume);
  for (auto channel : bus->AllChannels()) {
    SCOPED_TRACE("Half Scale");
    VerifySpanIsFilledWithValue(channel, kFillValue * kVolume);
  }

  // Verify zero volume case.
  bus->Scale(0);
  for (auto channel : bus->AllChannels()) {
    SCOPED_TRACE("Zero Scale");
    VerifySpanIsFilledWithValue(channel, 0);
  }
}

TEST_F(AudioBusTest, Bitstream) {
  static const size_t kDataSize = kFrameCount / 2;
  std::unique_ptr<AudioBus> bus = AudioBus::Create(1, kFrameCount);

  EXPECT_FALSE(bus->is_bitstream_format());
  bus->set_is_bitstream_format(true);
  EXPECT_TRUE(bus->is_bitstream_format());

  EXPECT_EQ(0u, bus->bitstream_data().size());
  bus->SetBitstreamSize(kDataSize);
  EXPECT_EQ(kDataSize, bus->bitstream_data().size());

  EXPECT_EQ(0, bus->GetBitstreamFrames());
  bus->SetBitstreamFrames(kFrameCount);
  EXPECT_EQ(static_cast<int>(kFrameCount), bus->GetBitstreamFrames());

  std::unique_ptr<AudioBus> bus2 = AudioBus::Create(1, kFrameCount);
  CopyTest(bus.get(), bus2.get());

  bus->Zero();
  EXPECT_EQ(0u, bus->bitstream_data().size());
  EXPECT_EQ(0, bus->GetBitstreamFrames());
}

}  // namespace media
