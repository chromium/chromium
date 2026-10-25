// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_array.h"

#include <algorithm>
#include <limits>

#include "base/memory/aligned_memory.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

TEST(AudioArrayTest, HandleOOM) {
  AudioArray<float> array;
  // Requesting an extremely large amount of memory to trigger OOM.
  // With the fix, TryAllocate should return false instead of crashing.
  // Use a value that is large but doesn't truncate to 0 on 32-bit systems.
  bool success = array.TryAllocate(std::numeric_limits<size_t>::max());
  EXPECT_FALSE(success);
}

TEST(AudioArrayTest, AlignmentAndFunctionality) {
  constexpr size_t kSize = 128;
  AudioFloatArray array(kSize);

  // Verify allocation success.
  EXPECT_NE(array.Data(), nullptr);
  EXPECT_EQ(array.size(), kSize);

  // Verify alignment (16B or 32B depending on architecture).
#if defined(ARCH_CPU_X86_FAMILY)
  EXPECT_TRUE(base::IsAligned(array.Data(), 32));
#else
  EXPECT_TRUE(base::IsAligned(array.Data(), 16));
#endif

  // Verify zero-initialization (BufferTryZeroedMalloc requirement).
  for (uint32_t i = 0; i < array.size(); ++i) {
    EXPECT_EQ(array[i], 0.0f);
  }

  // Verify write/read functionality.
  array[0] = 1.0f;
  array[kSize - 1] = 2.0f;
  EXPECT_EQ(array[0], 1.0f);
  EXPECT_EQ(array[kSize - 1], 2.0f);
}

TEST(AudioArrayTest, ZeroSizeAllocation) {
  AudioArray<float> array;
  // Should return true and result in a size of 0.
  EXPECT_TRUE(array.TryAllocate(0));
  EXPECT_EQ(array.size(), 0u);
  EXPECT_EQ(array.Data(), nullptr);
}

TEST(AudioBusTest, HandleOOM) {
  // Requesting a large amount of memory to trigger OOM in one of the channels.
  // TryCreate should return nullptr instead of crashing.
  scoped_refptr<AudioBus> bus = AudioBus::TryCreate(2, 1ULL << 31);
  EXPECT_EQ(bus, nullptr);
}

TEST(AudioBusTest, TryCreateTooManyChannels) {
  scoped_refptr<AudioBus> bus = AudioBus::TryCreate(33, 128);
  EXPECT_EQ(bus, nullptr);
}

TEST(AudioBusDeathTest, CreateTooManyChannels) {
  EXPECT_CHECK_DEATH(AudioBus::Create(33, 128, /*allocate=*/true));
  EXPECT_CHECK_DEATH(AudioBus::Create(33, 128, /*allocate=*/false));
}

TEST(AudioBusTest, TryCreateBySampleRateConvertingInvalidSampleRate) {
  scoped_refptr<AudioBus> bus = AudioBus::Create(1, 100);

  // Set an invalid sample rate (too low). This should fail with nullptr.
  bus->SetSampleRate(audio_utilities::MinAudioBufferSampleRate() - 1);
  EXPECT_EQ(
      AudioBus::TryCreateBySampleRateConverting(bus.get(), false, 48000),
      nullptr);

  // Set an invalid sample rate (too high). This should fail with nullptr.
  bus->SetSampleRate(audio_utilities::MaxAudioBufferSampleRate() + 1);
  EXPECT_EQ(
      AudioBus::TryCreateBySampleRateConverting(bus.get(), false, 48000),
      nullptr);

  // Set a valid source sample rate, and test invalid target sample rates.
  bus->SetSampleRate(44100);
  EXPECT_EQ(AudioBus::TryCreateBySampleRateConverting(bus.get(), false, 0),
            nullptr);
  EXPECT_EQ(AudioBus::TryCreateBySampleRateConverting(bus.get(), false, -44100),
            nullptr);
  EXPECT_EQ(
      AudioBus::TryCreateBySampleRateConverting(
          bus.get(), false, audio_utilities::MinAudioBufferSampleRate() - 1),
      nullptr);
  EXPECT_EQ(
      AudioBus::TryCreateBySampleRateConverting(
          bus.get(), false, audio_utilities::MaxAudioBufferSampleRate() + 1),
      nullptr);

  // Both source and target sample rates are valid.
  EXPECT_NE(
      AudioBus::TryCreateBySampleRateConverting(bus.get(), false, 48000),
      nullptr);
}

TEST(WebAudioBusTest, SafeCast) {
  WebAudioBus web_bus;
  // Requesting a length that is out of bounds for wtf_size_t (uint32_t).
  // TryInitialize should return false instead of crashing.
  // Use a value that is large but doesn't truncate to 0 on 32-bit systems.
  bool success =
      web_bus.TryInitialize(2, std::numeric_limits<size_t>::max(), 44100);
  EXPECT_FALSE(success);
}

namespace {

class FakeAudioDecodePlatform : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebAudioBus> DecodeAudioFileData(
      base::span<const char> audio_file_data) override {
    if (audio_file_data.empty()) {
      return nullptr;
    }
    auto web_bus = std::make_unique<WebAudioBus>();
    constexpr unsigned kChannels = 2;
    constexpr size_t kFrames = 128;
    constexpr double kNativeSampleRate = 44100.0;
    CHECK(web_bus->TryInitialize(kChannels, kFrames, kNativeSampleRate));
    std::ranges::fill(web_bus->ChannelData(0), 0.25f);
    std::ranges::fill(web_bus->ChannelData(1), 0.75f);
    return web_bus;
  }
};

}  // namespace

TEST(AudioBusTest, CreateBusFromInMemoryAudioFileNativeSampleRate) {
  ScopedTestingPlatformSupport<FakeAudioDecodePlatform> platform;
  const uint8_t fake_data[] = {1, 2, 3, 4};
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      fake_data, /*mix_to_mono=*/false, /*sample_rate=*/0.0f);
  ASSERT_NE(bus, nullptr);
  EXPECT_EQ(bus->SampleRate(), 44100.0f);
  EXPECT_EQ(bus->NumberOfChannels(), 2u);
  EXPECT_EQ(bus->length(), 128u);
  EXPECT_FLOAT_EQ(bus->Channel(0)->Span()[0], 0.25f);
  EXPECT_FLOAT_EQ(bus->Channel(1)->Span()[0], 0.75f);
}

TEST(AudioBusTest, CreateBusFromInMemoryAudioFileNativeSampleRateMixToMono) {
  ScopedTestingPlatformSupport<FakeAudioDecodePlatform> platform;
  const uint8_t fake_data[] = {1, 2, 3, 4};
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      fake_data, /*mix_to_mono=*/true, /*sample_rate=*/0.0f);
  ASSERT_NE(bus, nullptr);
  EXPECT_EQ(bus->SampleRate(), 44100.0f);
  EXPECT_EQ(bus->NumberOfChannels(), 1u);
  EXPECT_EQ(bus->length(), 128u);
  EXPECT_FLOAT_EQ(bus->Channel(0)->Span()[0], 0.5f);
}

TEST(AudioBusTest,
     CreateBusFromInMemoryAudioFileNegativeSampleRateReturnsNull) {
  ScopedTestingPlatformSupport<FakeAudioDecodePlatform> platform;
  const uint8_t fake_data[] = {1, 2, 3, 4};
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      fake_data, /*mix_to_mono=*/false, /*sample_rate=*/-44100.0f);
  EXPECT_EQ(bus, nullptr);
}

}  // namespace blink
