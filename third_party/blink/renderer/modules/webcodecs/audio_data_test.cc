// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"

#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_copy_to_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {
// Default test values
constexpr int64_t kTimestampInMicroSeconds = 1234;
constexpr int kChannels = 2;
constexpr int kFrames = 20;
constexpr int kSampleRate = 8000;
constexpr int kPartialFrameCount = 5;
constexpr int kOffset = 5;

constexpr int kMicrosecondPerSecond = 1E6;
constexpr uint64_t kExpectedDuration =
    static_cast<int64_t>(kFrames * kMicrosecondPerSecond / kSampleRate);

constexpr float kIncrement = 1.0f / 1000;
constexpr float kEpsilon = kIncrement / 100;
}  // namespace

class AudioDataTest : public testing::Test {
 protected:
  void VerifyPlanarData(float* data, float start_value, int count) {
    for (int i = 0; i < count; ++i)
      ASSERT_NEAR(data[i], start_value + i * kIncrement, kEpsilon) << "i=" << i;
  }

  AllowSharedBufferSource* CreateDefaultData() {
    return CreateCustomData(kChannels, kFrames);
  }

  AllowSharedBufferSource* CreateCustomData(int channels, int frames) {
    auto* buffer = DOMArrayBuffer::Create(channels * frames, sizeof(float));
    for (int ch = 0; ch < channels; ++ch) {
      float* plane_start =
          reinterpret_cast<float*>(buffer->Data()) + ch * frames;
      for (int i = 0; i < frames; ++i) {
        plane_start[i] = static_cast<float>((i + ch * frames) * kIncrement);
      }
    }
    return MakeGarbageCollected<AllowSharedBufferSource>(buffer);
  }

  AudioDataInit* CreateDefaultAudioDataInit(AllowSharedBufferSource* data) {
    auto* audio_data_init = AudioDataInit::Create();
    audio_data_init->setData(data);
    audio_data_init->setTimestamp(kTimestampInMicroSeconds);
    audio_data_init->setNumberOfChannels(kChannels);
    audio_data_init->setNumberOfFrames(kFrames);
    audio_data_init->setSampleRate(kSampleRate);
    audio_data_init->setFormat("f32-planar");
    return audio_data_init;
  }

  AudioData* CreateDefaultAudioData(ScriptState* script_state,
                                    ExceptionState& exception_state) {
    auto* data = CreateDefaultData();
    auto* audio_data_init = CreateDefaultAudioDataInit(data);
    return MakeGarbageCollected<AudioData>(script_state, audio_data_init,
                                           exception_state);
  }

  AudioDataCopyToOptions* CreateCopyToOptions(int index,
                                              absl::optional<uint32_t> offset,
                                              absl::optional<uint32_t> count) {
    auto* copy_to_options = AudioDataCopyToOptions::Create();
    copy_to_options->setPlaneIndex(index);

    if (offset.has_value())
      copy_to_options->setFrameOffset(offset.value());

    if (count.has_value())
      copy_to_options->setFrameCount(count.value());

    return copy_to_options;
  }

  void VerifyAllocationSize(int plane_index,
                            absl::optional<uint32_t> frame_offset,
                            absl::optional<uint32_t> frame_count,
                            bool should_throw,
                            int expected_size,
                            std::string description) {
    V8TestingScope scope;
    auto* frame = CreateDefaultAudioData(scope.GetScriptState(),
                                         scope.GetExceptionState());

    auto* options = CreateCopyToOptions(plane_index, frame_offset, frame_count);
    {
      SCOPED_TRACE(description);
      int allocations_size =
          frame->allocationSize(options, scope.GetExceptionState());

      EXPECT_EQ(should_throw, scope.GetExceptionState().HadException());
      EXPECT_EQ(allocations_size, expected_size);
    }
  }
};

TEST_F(AudioDataTest, ConstructFromMediaBuffer) {
  const media::ChannelLayout channel_layout =
      media::ChannelLayout::CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  constexpr base::TimeDelta timestamp =
      base::Microseconds(kTimestampInMicroSeconds);
  constexpr int kValueStart = 1;
  constexpr int kValueIncrement = 1;
  scoped_refptr<media::AudioBuffer> media_buffer =
      media::MakeAudioBuffer<int16_t>(
          media::SampleFormat::kSampleFormatS16, channel_layout, channels,
          kSampleRate, kValueStart, kValueIncrement, kFrames, timestamp);

  auto* frame = MakeGarbageCollected<AudioData>(media_buffer);

  EXPECT_EQ(frame->format(), "s16");
  EXPECT_EQ(frame->sampleRate(), static_cast<uint32_t>(kSampleRate));
  EXPECT_EQ(frame->numberOfFrames(), static_cast<uint32_t>(kFrames));
  EXPECT_EQ(frame->numberOfChannels(), static_cast<uint32_t>(kChannels));
  EXPECT_EQ(frame->duration(), kExpectedDuration);
  EXPECT_EQ(frame->timestamp(), kTimestampInMicroSeconds);

  // The media::AudioBuffer we receive should match the original |media_buffer|.
  EXPECT_EQ(frame->data(), media_buffer);

  frame->close();
  EXPECT_EQ(frame->data(), nullptr);
  EXPECT_EQ(frame->format(), absl::nullopt);
  EXPECT_EQ(frame->sampleRate(), 0u);
  EXPECT_EQ(frame->numberOfFrames(), 0u);
  EXPECT_EQ(frame->numberOfChannels(), 0u);
  EXPECT_EQ(frame->duration(), 0u);

  // Timestamp is preserved even after closing.
  EXPECT_EQ(frame->timestamp(), kTimestampInMicroSeconds);
}

TEST_F(AudioDataTest, ConstructFromAudioDataInit) {
  V8TestingScope scope;
  auto* buffer_source = CreateDefaultData();

  auto* audio_data_init = CreateDefaultAudioDataInit(buffer_source);

  auto* frame = MakeGarbageCollected<AudioData>(
      scope.GetScriptState(), audio_data_init, scope.GetExceptionState());

  EXPECT_EQ(frame->format(), "f32-planar");
  EXPECT_EQ(frame->sampleRate(), static_cast<uint32_t>(kSampleRate));
  EXPECT_EQ(frame->numberOfFrames(), static_cast<uint32_t>(kFrames));
  EXPECT_EQ(frame->numberOfChannels(), static_cast<uint32_t>(kChannels));
  EXPECT_EQ(frame->duration(), kExpectedDuration);
  EXPECT_EQ(frame->timestamp(), kTimestampInMicroSeconds);
}

TEST_F(AudioDataTest, ConstructFromAudioDataInit_HighChannelCount) {
  V8TestingScope scope;
  constexpr int kHighChannelCount = 25;
  auto* buffer_source = CreateCustomData(kHighChannelCount, kFrames);

  auto* audio_data_init = CreateDefaultAudioDataInit(buffer_source);
  audio_data_init->setNumberOfChannels(kHighChannelCount);

  auto* frame = MakeGarbageCollected<AudioData>(
      scope.GetScriptState(), audio_data_init, scope.GetExceptionState());

  EXPECT_EQ(frame->format(), "f32-planar");
  EXPECT_EQ(frame->sampleRate(), static_cast<uint32_t>(kSampleRate));
  EXPECT_EQ(frame->numberOfFrames(), static_cast<uint32_t>(kFrames));
  EXPECT_EQ(frame->numberOfChannels(),
            static_cast<uint32_t>(kHighChannelCount));
  EXPECT_EQ(frame->duration(), kExpectedDuration);
  EXPECT_EQ(frame->timestamp(), kTimestampInMicroSeconds);
}

TEST_F(AudioDataTest, AllocationSize) {
  // We only support the "FLTP" format for now.
  constexpr int kTotalSizeInBytes = kFrames * sizeof(float);

  // Basic cases.
  VerifyAllocationSize(0, absl::nullopt, absl::nullopt, false,
                       kTotalSizeInBytes, "Default");
  VerifyAllocationSize(1, absl::nullopt, absl::nullopt, false,
                       kTotalSizeInBytes, "Valid index.");
  VerifyAllocationSize(0, 0, kFrames, false, kTotalSizeInBytes,
                       "Specifying defaults");

  // Cases where we cover a subset of samples.
  VerifyAllocationSize(0, kFrames / 2, absl::nullopt, false,
                       kTotalSizeInBytes / 2, "Valid offset, no count");
  VerifyAllocationSize(0, kFrames / 2, kFrames / 4, false,
                       kTotalSizeInBytes / 4, "Valid offset and count");
  VerifyAllocationSize(0, absl::nullopt, kFrames / 2, false,
                       kTotalSizeInBytes / 2, "No offset, valid count");

  // Copying 0 frames is technically valid.
  VerifyAllocationSize(0, absl::nullopt, 0, false, 0, "Frame count is 0");

  // Failures
  VerifyAllocationSize(2, absl::nullopt, absl::nullopt, true, 0,
                       "Invalid index.");
  VerifyAllocationSize(0, kFrames, absl::nullopt, true, 0, "Offset too big");
  VerifyAllocationSize(0, absl::nullopt, kFrames + 1, true, 0, "Count too big");
  VerifyAllocationSize(0, 1, kFrames, true, 0, "Count too big, with offset");
}

TEST_F(AudioDataTest, CopyTo_DestinationTooSmall) {
  V8TestingScope scope;
  auto* frame =
      CreateDefaultAudioData(scope.GetScriptState(), scope.GetExceptionState());
  auto* options = CreateCopyToOptions(/*index=*/0, /*offset=*/absl::nullopt,
                                      /*count=*/absl::nullopt);

  AllowSharedBufferSource* small_dest =
      MakeGarbageCollected<AllowSharedBufferSource>(
          DOMArrayBuffer::Create(kFrames - 1, sizeof(float)));

  frame->copyTo(small_dest, options, scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST_F(AudioDataTest, CopyTo_FullFrames) {
  V8TestingScope scope;
  auto* frame =
      CreateDefaultAudioData(scope.GetScriptState(), scope.GetExceptionState());
  auto* options = CreateCopyToOptions(/*index=*/0, /*offset=*/absl::nullopt,
                                      /*count=*/absl::nullopt);

  DOMArrayBuffer* data_copy = DOMArrayBuffer::Create(kFrames, sizeof(float));
  AllowSharedBufferSource* dest =
      MakeGarbageCollected<AllowSharedBufferSource>(data_copy);

  // All frames should have been copied.
  frame->copyTo(dest, options, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  VerifyPlanarData(static_cast<float*>(data_copy->Data()), /*start_value=*/0,
                   /*count=*/kFrames);
}

TEST_F(AudioDataTest, CopyTo_PlaneIndex) {
  V8TestingScope scope;
  auto* frame =
      CreateDefaultAudioData(scope.GetScriptState(), scope.GetExceptionState());
  auto* options = CreateCopyToOptions(/*index=*/1, /*offset=*/absl::nullopt,
                                      /*count=*/absl::nullopt);

  DOMArrayBuffer* data_copy = DOMArrayBuffer::Create(kFrames, sizeof(float));
  AllowSharedBufferSource* dest =
      MakeGarbageCollected<AllowSharedBufferSource>(data_copy);

  // All frames should have been copied.
  frame->copyTo(dest, options, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  // The channel 1's start value is kFrames*in.
  VerifyPlanarData(static_cast<float*>(data_copy->Data()),
                   /*start_value=*/kFrames * kIncrement,
                   /*count=*/kFrames);
}

TEST_F(AudioDataTest, TransferBuffer) {
  V8TestingScope scope;
  std::string data = "audio data";
  auto* buffer = DOMArrayBuffer::Create(data.data(), data.size());
  auto* buffer_source = MakeGarbageCollected<AllowSharedBufferSource>(buffer);

  auto* audio_data_init = AudioDataInit::Create();
  audio_data_init->setData(buffer_source);
  audio_data_init->setTimestamp(0);
  audio_data_init->setNumberOfChannels(1);
  audio_data_init->setNumberOfFrames(static_cast<uint32_t>(data.size()));
  audio_data_init->setSampleRate(kSampleRate);
  audio_data_init->setFormat("u8");
  HeapVector<Member<DOMArrayBuffer>> transfer;
  transfer.push_back(Member<DOMArrayBuffer>(buffer));
  audio_data_init->setTransfer(std::move(transfer));

  auto* frame = MakeGarbageCollected<AudioData>(
      scope.GetScriptState(), audio_data_init, scope.GetExceptionState());

  EXPECT_EQ(frame->format(), "u8");
  EXPECT_EQ(frame->numberOfFrames(), data.size());
  EXPECT_EQ(frame->numberOfChannels(), 1u);

  auto* options = CreateCopyToOptions(0, 0, {});
  uint32_t allocations_size =
      frame->allocationSize(options, scope.GetExceptionState());

  EXPECT_TRUE(buffer->IsDetached());
  EXPECT_EQ(allocations_size, data.size());
}

TEST_F(AudioDataTest, CopyTo_Offset) {
  V8TestingScope scope;

  auto* frame =
      CreateDefaultAudioData(scope.GetScriptState(), scope.GetExceptionState());
  auto* options =
      CreateCopyToOptions(/*index=*/0, kOffset, /*count=*/absl::nullopt);

  // |data_copy| is bigger than what we need, and that's ok.
  DOMArrayBuffer* data_copy = DOMArrayBuffer::Create(kFrames, sizeof(float));
  AllowSharedBufferSource* dest =
      MakeGarbageCollected<AllowSharedBufferSource>(data_copy);

  // All frames should have been copied.
  frame->copyTo(dest, options, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  VerifyPlanarData(static_cast<float*>(data_copy->Data()),
                   /*start_value=*/kOffset * kIncrement,
                   /*count=*/kFrames - kOffset);
}

TEST_F(AudioDataTest, CopyTo_PartialFrames) {
  V8TestingScope scope;

  auto* frame =
      CreateDefaultAudioData(scope.GetScriptState(), scope.GetExceptionState());
  auto* options = CreateCopyToOptions(/*index=*/0, /*offset=*/absl::nullopt,
                                      kPartialFrameCount);

  DOMArrayBuffer* data_copy =
      DOMArrayBuffer::Create(kPartialFrameCount, sizeof(float));
  AllowSharedBufferSource* dest =
      MakeGarbageCollected<AllowSharedBufferSource>(data_copy);

  // All frames should have been copied.
  frame->copyTo(dest, options, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  VerifyPlanarData(static_cast<float*>(data_copy->Data()),
                   /*start_value=*/0, kPartialFrameCount);
}

TEST_F(AudioDataTest, CopyTo_PartialFramesAndOffset) {
  V8TestingScope scope;

  auto* frame =
      CreateDefaultAudioData(scope.GetScriptState(), scope.GetExceptionState());
  auto* options = CreateCopyToOptions(/*index=*/0, kOffset, kPartialFrameCount);

  DOMArrayBuffer* data_copy =
      DOMArrayBuffer::Create(kPartialFrameCount, sizeof(float));
  AllowSharedBufferSource* dest =
      MakeGarbageCollected<AllowSharedBufferSource>(data_copy);

  // All frames should have been copied.
  frame->copyTo(dest, options, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  VerifyPlanarData(static_cast<float*>(data_copy->Data()),
                   /*start_value=*/kOffset * kIncrement, kPartialFrameCount);
}

TEST_F(AudioDataTest, Interleaved) {
  V8TestingScope scope;

  // Do not use a power of 2, to make it easier to verify the allocationSize()
  // results.
  constexpr int kInterleavedChannels = 3;

  std::vector<int16_t> samples(kFrames * kInterleavedChannels);

  // Populate samples.
  for (int i = 0; i < kFrames; ++i) {
    int block_index = i * kInterleavedChannels;

    samples[block_index] = i;                    // channel 0
    samples[block_index + 1] = i + kFrames;      // channel 1
    samples[block_index + 2] = i + 2 * kFrames;  // channel 2
  }

  const uint8_t* data[] = {reinterpret_cast<const uint8_t*>(samples.data())};

  auto media_buffer = media::AudioBuffer::CopyFrom(
      media::SampleFormat::kSampleFormatS16,
      media::GuessChannelLayout(kInterleavedChannels), kInterleavedChannels,
      kSampleRate, kFrames, data, base::TimeDelta());

  auto* frame = MakeGarbageCollected<AudioData>(media_buffer);

  EXPECT_EQ("s16", frame->format());

  // Verify that plane indexes > 1 throw, for interleaved formats.
  auto* options = CreateCopyToOptions(/*index=*/1, kOffset, kPartialFrameCount);
  int allocations_size =
      frame->allocationSize(options, scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  scope.GetExceptionState().ClearException();

  // Verify that copy conversion to a planar format supports indexes > 1,
  // even if the source is interleaved.
  options->setFormat(V8AudioSampleFormat::Enum::kF32Planar);
  allocations_size = frame->allocationSize(options, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  // Verify we get the expected allocation size, for valid formats.
  options = CreateCopyToOptions(/*index=*/0, kOffset, kPartialFrameCount);
  allocations_size = frame->allocationSize(options, scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  // Interleaved formats take into account the number of channels.
  EXPECT_EQ(static_cast<unsigned int>(allocations_size),
            kPartialFrameCount * kInterleavedChannels * sizeof(uint16_t));

  DOMArrayBuffer* data_copy = DOMArrayBuffer::Create(
      kPartialFrameCount * kInterleavedChannels, sizeof(uint16_t));
  AllowSharedBufferSource* dest =
      MakeGarbageCollected<AllowSharedBufferSource>(data_copy);

  // All frames should have been copied.
  frame->copyTo(dest, options, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  // Verify we retrieved the right samples.
  int16_t* copy = static_cast<int16_t*>(data_copy->Data());
  for (int i = 0; i < kPartialFrameCount; ++i) {
    int block_index = i * kInterleavedChannels;
    int16_t base_value = kOffset + i;

    EXPECT_EQ(copy[block_index], base_value);                    // channel 0
    EXPECT_EQ(copy[block_index + 1], base_value + kFrames);      // channel 1
    EXPECT_EQ(copy[block_index + 2], base_value + 2 * kFrames);  // channel 2
  }
}

}  // namespace blink
