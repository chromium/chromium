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
#include "base/types/zip.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
using AlignedFloatArray = base::AlignedHeapArray<float>;

static constexpr int kDefaultChannels = 6;
static constexpr ChannelLayout kChannelLayout = CHANNEL_LAYOUT_5_1;
// Use a buffer size which is intentionally not a multiple of kChannelAlignment.
static constexpr size_t kFrameCount =
    media::AudioBus::kChannelAlignment * 32 - 1;
static constexpr int kSampleRate = 48000;
static constexpr float kGuardValue = 123.456f;

// Verify values for each channel in |result| are within |epsilon| of
// |expected|.  If |epsilon| exactly equals 0, uses FLOAT_EQ macro.
void VerifyAreEqualWithEpsilon(const AudioBus* result,
                               const AudioBus* expected,
                               float epsilon) {
  EXPECT_EQ(expected->channels(), result->channels());
  EXPECT_EQ(expected->frames(), result->frames());
  EXPECT_EQ(expected->is_bitstream_format(), result->is_bitstream_format());

  if (expected->is_bitstream_format()) {
    EXPECT_EQ(expected->GetBitstreamFrames(), result->GetBitstreamFrames());
    EXPECT_EQ(expected->bitstream_data(), result->bitstream_data());
    return;
  }

  for (int ch = 0; ch < result->channels(); ++ch) {
    for (int i = 0; i < result->frames(); ++i) {
      SCOPED_TRACE(base::StringPrintf("ch=%d, i=%d", ch, i));

      if (epsilon == 0) {
        ASSERT_FLOAT_EQ(expected->channel(ch)[i], result->channel(ch)[i]);
      } else {
        ASSERT_NEAR(expected->channel(ch)[i], result->channel(ch)[i], epsilon);
      }
    }
  }
}
}  // namespace

class AudioBusTest : public testing::Test {
 public:
  AudioBusTest() = default;

  AudioBusTest(const AudioBusTest&) = delete;
  AudioBusTest& operator=(const AudioBusTest&) = delete;

  ~AudioBusTest() override = default;

  void VerifyChannelAndFrameCount(AudioBus* bus) {
    EXPECT_EQ(kDefaultChannels, bus->channels());
    EXPECT_EQ(static_cast<int>(kFrameCount), bus->frames());
  }

  void VerifySpanIsFilledWithValue(AudioBus::ConstChannel data, float value) {
    for (size_t i = 0; i < data.size(); ++i) {
      ASSERT_FLOAT_EQ(value, data[i]) << "i=" << i;
    }
  }

  // Verify values for each channel in |result| against |expected|.
  void VerifyAreEqual(const AudioBus* result, const AudioBus* expected) {
    VerifyAreEqualWithEpsilon(result, expected, 0);
  }

  // Read and write to the full extent of the allocated channel data.  Also
  // test the Zero() method and verify it does as advertised.  Also test data
  // if data is 16-byte aligned as advertised (see kChannelAlignment in
  // audio_bus.h).
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
      VerifySpanIsFilledWithValue(bus->channel(i), i);
    }

    bus->Zero();
    for (int i = 0; i < bus->channels(); ++i) {
      VerifySpanIsFilledWithValue(bus->channel(i), 0);
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
    data_.reserve(kDefaultChannels);
    for (int i = 0; i < kDefaultChannels; ++i) {
      data_.push_back(
          base::AlignedUninit<float>(kFrameCount, AudioBus::kChannelAlignment));
    }
  }

  std::vector<AlignedFloatArray> data_;
};

// Verify basic Create(...) method works as advertised.
TEST_F(AudioBusTest, Create) {
  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(kDefaultChannels, kFrameCount);
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

  std::unique_ptr<AudioBus> bus = AudioBus::CreateWrapper(kDefaultChannels);
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

  std::unique_ptr<AudioBus> bus = AudioBus::CreateWrapper(kDefaultChannels);
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
    EXPECT_EQ(channel, bus->channel(current_channel++));
  }

  EXPECT_EQ(current_channel, kDefaultChannels);
}

TEST_F(AudioBusTest, AllChannelsSubspan) {
  AllocateDataPerChannel();

  std::unique_ptr<AudioBus> bus = AudioBus::CreateWrapper(kDefaultChannels);
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
              bus->channel(current_channel++).subspan(kOffset, kCount));
  }

  EXPECT_EQ(current_channel, kDefaultChannels);
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
  std::unique_ptr<AudioBus> bus1 =
      AudioBus::Create(kDefaultChannels, kFrameCount);
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
  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(kDefaultChannels, kFrameCount);

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
static constexpr size_t kFrames = 5;
static constexpr size_t kChannels = 2;
static constexpr size_t kTotalFrames = kFrames * kChannels;
static const std::array<uint8_t, kTotalFrames> kTestVectorUint8 = {
    0,         -INT8_MIN,          UINT8_MAX,
    0,         INT8_MAX / 2 + 128, INT8_MIN / 2 + 128,
    -INT8_MIN, UINT8_MAX,          -INT8_MIN,
    -INT8_MIN};
static const std::array<int16_t, kTotalFrames> kTestVectorInt16 = {
    INT16_MIN,     0, INT16_MAX, INT16_MIN, INT16_MAX / 2,
    INT16_MIN / 2, 0, INT16_MAX, 0,         0};
static const std::array<int32_t, kTotalFrames> kTestVectorInt32 = {
    INT32_MIN,     0, INT32_MAX, INT32_MIN, INT32_MAX / 2,
    INT32_MIN / 2, 0, INT32_MAX, 0,         0};
static const std::array<float, kTotalFrames> kTestVectorFloat32 = {
    -1.0f, 0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f};

// This is based on kTestVectorFloat32, but has some of the values outside of
// sanity.
static const std::array<float, kTotalFrames> kTestVectorFloat32Invalid = {
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

static const std::array<float, kTotalFrames> kTestVectorFloat32Sanitized = {
    -1.0f, 0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f};

// Expected results.
static const auto kExpectedPlanarData =
    std::to_array<std::array<const float, kFrames>>(
        {{-1.0f, 1.0f, 0.5f, 0.0f, 0.0f}, {0.0f, -1.0f, -0.5f, 1.0f, 0.0f}});

template <class SampleTypeTraits>
class TypedAudioBusTest : public testing::Test {
 public:
  using SampleType = typename SampleTypeTraits::ValueType;
  using InterleavedArray = std::array<SampleType, kTotalFrames>;

  auto GetInterleavedSpan() {
    if constexpr (std::is_same_v<SampleType, uint8_t>) {
      return base::span(kTestVectorUint8);
    } else if constexpr (std::is_same_v<SampleType, int16_t>) {
      return base::span(kTestVectorInt16);
    } else if constexpr (std::is_same_v<SampleType, int32_t>) {
      return base::span(kTestVectorInt32);
    } else if constexpr (std::is_same_v<SampleType, float>) {
      return base::span(kTestVectorFloat32);
    }
  }

  // Some compilers get better precision than others on the half-max test, so
  // let the test pass with an off by one check on the half-max.
  auto GetAlternateValidResult() {
    if constexpr (std::is_same_v<SampleType, int32_t>) {
      InterleavedArray array;
      base::span(array).copy_from_nonoverlapping(kTestVectorInt32);
      EXPECT_EQ(array[4], std::numeric_limits<int32_t>::max() / 2);
      array[4] = std::numeric_limits<int32_t>::max() / 2 + 1;
      return array;
    } else {
      NOTREACHED();
    }
  }

  std::unique_ptr<AudioBus> GetPlanarDataBus() {
    auto bus = AudioBus::Create(kChannels, kFrames);
    auto channels = bus->AllChannels();
    for (auto [channel, expected] : base::zip(channels, kExpectedPlanarData)) {
      channel.copy_from_nonoverlapping(expected);
    }
    return bus;
  }

  std::unique_ptr<AudioBus> GetTestBus(float fill_value = 0) {
    auto bus = AudioBus::Create(kChannels, kFrames);
    if (fill_value == 0) {
      bus->Zero();
    } else {
      for (auto channel : bus->AllChannels()) {
        std::ranges::fill(channel, fill_value);
      }
    }
    return bus;
  }

  constexpr base::span<uint8_t> as_writable_bytes(base::span<SampleType> span) {
    if constexpr (std::is_same_v<SampleType, float>) {
      return base::as_writable_bytes(base::allow_nonunique_obj, span);
    } else {
      return base::as_writable_bytes(span);
    }
  }

  constexpr base::span<const uint8_t> as_bytes(
      base::span<const SampleType> span) {
    if constexpr (std::is_same_v<SampleType, float>) {
      return base::as_bytes(base::allow_nonunique_obj, span);
    } else {
      return base::as_bytes(span);
    }
  }

  constexpr float GetEpsilon() {
    if constexpr (std::is_same_v<SampleType, uint8_t>) {
      // Biased uint8_t calculations have poor precision, so the epsilon here is
      // slightly more permissive than int16_t and int32_t calculations.
      return 1.0f / (std::numeric_limits<uint8_t>::max() - 1);
    } else if constexpr (std::is_same_v<SampleType, int16_t>) {
      return 1.0f /
             (static_cast<float>(std::numeric_limits<uint16_t>::max()) + 1.0f);
    } else if constexpr (std::is_same_v<SampleType, int32_t>) {
      return 1.0f / static_cast<float>(std::numeric_limits<uint32_t>::max());
    } else if constexpr (std::is_same_v<SampleType, float>) {
      return 0;
    }
  }
};

class TypedTestNameGenerator {
 public:
  template <typename T>
  static std::string GetName(int) {
    if constexpr (std::is_same_v<T, UnsignedInt8SampleTypeTraits>) {
      return "U8";
    }
    if constexpr (std::is_same_v<T, SignedInt16SampleTypeTraits>) {
      return "S16";
    }
    if constexpr (std::is_same_v<T, SignedInt32SampleTypeTraits>) {
      return "S32";
    }
    if constexpr (std::is_same_v<T, Float32SampleTypeTraits>) {
      return "F32";
    }
  }
};

using SampleTypes = testing::Types<UnsignedInt8SampleTypeTraits,
                                   SignedInt16SampleTypeTraits,
                                   SignedInt32SampleTypeTraits,
                                   Float32SampleTypeTraits>;
TYPED_TEST_SUITE(TypedAudioBusTest, SampleTypes, TypedTestNameGenerator);

// Verify FromInterleaved() deinterleaves audio in supported formats correctly.
TYPED_TEST(TypedAudioBusTest, FromInterleaved) {
  using SampleTypeTraits = TypeParam;
  const std::unique_ptr<AudioBus> expected = this->GetPlanarDataBus();
  {
    SCOPED_TRACE("Safe");
    std::unique_ptr<AudioBus> bus = this->GetTestBus();
    bus->template FromInterleaved<SampleTypeTraits>(this->GetInterleavedSpan());
    VerifyAreEqualWithEpsilon(bus.get(), expected.get(), this->GetEpsilon());
  }
  {
    SCOPED_TRACE("FromBytes");
    std::unique_ptr<AudioBus> bus = this->GetTestBus();
    bus->template FromInterleavedBytes<SampleTypeTraits>(
        this->as_bytes(this->GetInterleavedSpan()));
    VerifyAreEqualWithEpsilon(bus.get(), expected.get(), this->GetEpsilon());
  }
  {
    SCOPED_TRACE("Unsafe");
    std::unique_ptr<AudioBus> bus = this->GetTestBus();
    bus->template FromInterleaved<SampleTypeTraits>(
        this->GetInterleavedSpan().data(), kFrames);
    VerifyAreEqualWithEpsilon(bus.get(), expected.get(), this->GetEpsilon());
  }
}

// Verify FromInterleaved() properly zeros remaining frames when
// `zero_remaining_frames` is set.
TYPED_TEST(TypedAudioBusTest, FromInterleaved_Zero) {
  using SampleTypeTraits = TypeParam;

  // Zero out the end of the expected values.
  const size_t kPartialFrames = 3;
  std::unique_ptr<AudioBus> expected = this->GetPlanarDataBus();
  for (auto channel : expected->AllChannels()) {
    auto [data, zero] = channel.template split_at<kPartialFrames>();
    std::ranges::fill(zero, 0);
  }

  auto interleaved_subspan =
      this->GetInterleavedSpan().first(kChannels * kPartialFrames);

  {
    SCOPED_TRACE("Safe");
    // Fill with guard values that should be zero'ed out.
    std::unique_ptr<AudioBus> bus =
        this->GetTestBus(/*fill_value=*/kGuardValue);
    bus->template FromInterleaved<SampleTypeTraits>(
        interleaved_subspan,
        /*zero_remaining_frames=*/true);
    VerifyAreEqualWithEpsilon(bus.get(), expected.get(), this->GetEpsilon());
  }
  {
    SCOPED_TRACE("FromBytes");
    // Fill with guard values that should be zero'ed out.
    std::unique_ptr<AudioBus> bus =
        this->GetTestBus(/*fill_value=*/kGuardValue);
    bus->template FromInterleavedBytes<SampleTypeTraits>(
        this->as_bytes(interleaved_subspan),
        /*zero_remaining_frames=*/true);
    VerifyAreEqualWithEpsilon(bus.get(), expected.get(), this->GetEpsilon());
  }
  {
    SCOPED_TRACE("Unsafe");
    // Fill with guard values that should be zero'ed out.
    std::unique_ptr<AudioBus> bus =
        this->GetTestBus(/*fill_value=*/kGuardValue);
    bus->template FromInterleaved<SampleTypeTraits>(interleaved_subspan.data(),
                                                    kPartialFrames);
    VerifyAreEqualWithEpsilon(bus.get(), expected.get(), this->GetEpsilon());
  }
}

// Verify FromInterleaved() deinterleaves audio in supported formats correctly.
TYPED_TEST(TypedAudioBusTest, FromInterleavedPartial) {
  using SampleTypeTraits = TypeParam;
  using SampleType = SampleTypeTraits::ValueType;

  static constexpr size_t kOffset = 1;
  static constexpr size_t kCount = 2;
  static constexpr size_t kTotalOffset = kChannels * kOffset;
  static constexpr size_t kTotalCount = kChannels * kCount;

  // Fill every span but `data` with guard values, as these shouldn't be
  // overwritten by FromInterleavedPartial().
  std::unique_ptr<AudioBus> expected = this->GetPlanarDataBus();
  for (auto channel : expected->AllChannels()) {
    auto [head, rem] = channel.template split_at<kOffset>();
    auto [data, tail] = rem.template split_at<kCount>();
    std::ranges::fill(head, kGuardValue);
    std::ranges::fill(tail, kGuardValue);
  }

  base::span<const SampleType> interleaved_data =
      this->GetInterleavedSpan().subspan(kTotalOffset, kTotalCount);

  {
    SCOPED_TRACE("Safe");
    // Fill with guard values that should untouched.
    std::unique_ptr<AudioBus> bus =
        this->GetTestBus(/*fill_value=*/kGuardValue);
    bus->template FromInterleavedPartial<SampleTypeTraits>(interleaved_data,
                                                           kOffset);

    VerifyAreEqualWithEpsilon(bus.get(), expected.get(), this->GetEpsilon());
  }
  {
    SCOPED_TRACE("FromBytes");
    // Fill with guard values that should untouched.
    std::unique_ptr<AudioBus> bus =
        this->GetTestBus(/*fill_value=*/kGuardValue);
    bus->template FromInterleavedBytesPartial<SampleTypeTraits>(
        this->as_bytes(interleaved_data), kOffset);

    VerifyAreEqualWithEpsilon(bus.get(), expected.get(), this->GetEpsilon());
  }
  {
    SCOPED_TRACE("Unsafe");
    // Fill with guard values that should untouched.
    std::unique_ptr<AudioBus> bus =
        this->GetTestBus(/*fill_value=*/kGuardValue);
    bus->template FromInterleavedPartial<SampleTypeTraits>(
        interleaved_data.data(), kOffset, kCount);

    VerifyAreEqualWithEpsilon(bus.get(), expected.get(), this->GetEpsilon());
  }
}

// Verify ToInterleaved() interleaves audio in supported formats correctly.
TYPED_TEST(TypedAudioBusTest, ToInterleaved) {
  using SampleTypeTraits = TypeParam;
  using ValueType = typename SampleTypeTraits::ValueType;
  using TestArray = TypedAudioBusTest<SampleTypeTraits>::InterleavedArray;

  base::span<const ValueType> expected_span = this->GetInterleavedSpan();
  auto verify_array = [&](const TestArray& dest_array) {
    if constexpr (std::is_same_v<ValueType, int32_t>) {
      ASSERT_TRUE(base::span(dest_array) == expected_span ||
                  base::span(dest_array) == this->GetAlternateValidResult());
    } else {
      ASSERT_EQ(base::span(dest_array), expected_span);
    }
  };

  std::unique_ptr<AudioBus> source_bus = this->GetPlanarDataBus();
  {
    SCOPED_TRACE("Safe");
    TestArray dest_array;
    source_bus->template ToInterleaved<SampleTypeTraits>(dest_array);
    verify_array(dest_array);
  }
  {
    SCOPED_TRACE("ToBytes");
    TestArray dest_array;
    source_bus->template ToInterleavedBytes<SampleTypeTraits>(
        this->as_writable_bytes(dest_array));
    verify_array(dest_array);
  }
  {
    SCOPED_TRACE("Unsafe");
    TestArray dest_array;
    source_bus->template ToInterleaved<SampleTypeTraits>(kFrames,
                                                         dest_array.data());
    verify_array(dest_array);
  }
}

// Verify ToInterleaved() interleaves audio in supported formats correctly.
TYPED_TEST(TypedAudioBusTest, ToInterleavedPartial) {
  using SampleTypeTraits = TypeParam;
  using ValueType = typename SampleTypeTraits::ValueType;
  static constexpr size_t kOffset = 1;
  static constexpr size_t kCount = 2;
  static constexpr size_t kTotalOffset = kChannels * kOffset;
  static constexpr size_t kTotalCount = kChannels * kCount;
  using TestArray = std::array<ValueType, kTotalCount>;
  base::span<const ValueType> expected_span =
      this->GetInterleavedSpan().subspan(kTotalOffset, kTotalCount);

  auto verify_array = [&](const TestArray& dest_array) {
    if constexpr (std::is_same_v<ValueType, int32_t>) {
      auto alternate_valid_data = this->GetAlternateValidResult();
      auto alternate_span =
          base::span(alternate_valid_data).subspan(kTotalOffset, kTotalCount);
      ASSERT_TRUE(base::span(dest_array) == expected_span ||
                  base::span(dest_array) == alternate_span);
    } else {
      ASSERT_EQ(base::span(dest_array), expected_span);
    }
  };

  std::unique_ptr<AudioBus> source_bus = this->GetPlanarDataBus();
  {
    SCOPED_TRACE("Safe");
    TestArray dest_array;
    source_bus->template ToInterleavedPartial<SampleTypeTraits>(kOffset,
                                                                dest_array);
    verify_array(dest_array);
  }
  {
    SCOPED_TRACE("ToBytes");
    TestArray dest_array;
    source_bus->template ToInterleavedBytesPartial<SampleTypeTraits>(
        kOffset, this->as_writable_bytes(dest_array));
    verify_array(dest_array);
  }
  {
    SCOPED_TRACE("Unsafe");
    TestArray dest_array;
    source_bus->template ToInterleavedPartial<SampleTypeTraits>(
        kOffset, kCount, dest_array.data());
    verify_array(dest_array);
  }
}

TEST_F(AudioBusTest, ToInterleavedSanitized) {
  std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrames);
  bus->FromInterleaved<Float32SampleTypeTraits>(kTestVectorFloat32Invalid,
                                                bus->frames());
  // Verify FromInterleaved applied no sanitization, for test setup.
  ASSERT_EQ(bus->channel(0)[0], kTestVectorFloat32Invalid[0]);

  // VerifyToInterleaved applied sanitization.
  std::array<float, std::size(kTestVectorFloat32Sanitized)> test_array;
  bus->ToInterleaved<Float32SampleTypeTraits>(test_array);
  ASSERT_EQ(kTestVectorFloat32Sanitized, test_array);

  // Verify that Float32SampleTypeTraitsNoClip applied no sanity. Note: We don't
  // use memcmp() here since the NaN type may change on x86 platforms in certain
  // circumstances, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57484
  bus->ToInterleaved<Float32SampleTypeTraitsNoClip>(test_array);
  for (size_t i = 0; i < kTotalFrames; ++i) {
    if (std::isnan(test_array[i])) {
      EXPECT_TRUE(std::isnan(kTestVectorFloat32Invalid[i]));
    } else {
      EXPECT_FLOAT_EQ(test_array[i], kTestVectorFloat32Invalid[i]);
    }
  }
}

TEST_F(AudioBusTest, ToInterleavedSanitized_Unsafe) {
  std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrames);
  bus->FromInterleaved<Float32SampleTypeTraits>(
      kTestVectorFloat32Invalid.data(), bus->frames());
  // Verify FromInterleaved applied no sanity.
  ASSERT_EQ(bus->channel(0)[0], kTestVectorFloat32Invalid[0]);
  std::array<float, std::size(kTestVectorFloat32Sanitized)> test_array;
  bus->ToInterleaved<Float32SampleTypeTraits>(bus->frames(), test_array.data());
  for (size_t i = 0; i < std::size(kTestVectorFloat32Sanitized); ++i) {
    ASSERT_EQ(kTestVectorFloat32Sanitized[i], test_array[i]);
  }

  // Verify that Float32SampleTypeTraitsNoClip applied no sanity. Note: We don't
  // use memcmp() here since the NaN type may change on x86 platforms in certain
  // circumstances, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57484
  bus->ToInterleaved<Float32SampleTypeTraitsNoClip>(bus->frames(),
                                                    test_array.data());
  for (size_t i = 0; i < kTotalFrames; ++i) {
    if (std::isnan(test_array[i])) {
      EXPECT_TRUE(std::isnan(kTestVectorFloat32Invalid[i]));
    } else {
      EXPECT_FLOAT_EQ(test_array[i], kTestVectorFloat32Invalid[i]);
    }
  }
}

TEST_F(AudioBusTest, CopyAndClipTo) {
  auto bus = AudioBus::Create(kChannels, kFrames);
  bus->FromInterleaved<Float32SampleTypeTraits>(kTestVectorFloat32Invalid,
                                                bus->frames());
  auto expected = AudioBus::Create(kChannels, kFrames);
  expected->FromInterleaved<Float32SampleTypeTraits>(
      kTestVectorFloat32Sanitized, bus->frames());

  // Verify FromInterleaved applied no sanity.
  ASSERT_EQ(bus->channel(0)[0], kTestVectorFloat32Invalid[0]);

  std::unique_ptr<AudioBus> copy_to_bus = AudioBus::Create(kChannels, kFrames);
  bus->CopyAndClipTo(copy_to_bus.get());

  for (int ch = 0; ch < expected->channels(); ++ch) {
    for (int i = 0; i < expected->frames(); ++i) {
      ASSERT_EQ(copy_to_bus->channel(ch)[i], expected->channel(ch)[i]);
    }
  }

  ASSERT_EQ(expected->channels(), copy_to_bus->channels());
  ASSERT_EQ(expected->frames(), copy_to_bus->frames());
  ASSERT_EQ(expected->is_bitstream_format(),
            copy_to_bus->is_bitstream_format());
}

TEST_F(AudioBusTest, Scale) {
  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(kDefaultChannels, kFrameCount);

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
