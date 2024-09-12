// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"

#include <optional>

#include "base/containers/span.h"
#include "media/base/audio_sample_types.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_copy_to_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_sample_format.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
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
                                              std::optional<uint32_t> offset,
                                              std::optional<uint32_t> count) {
    auto* copy_to_options = AudioDataCopyToOptions::Create();
    copy_to_options->setPlaneIndex(index);

    if (offset.has_value())
      copy_to_options->setFrameOffset(offset.value());

    if (count.has_value())
      copy_to_options->setFrameCount(count.value());

    return copy_to_options;
  }

  void VerifyAllocationSize(int plane_index,
                            std::optional<uint32_t> frame_offset,
                            std::optional<uint32_t> frame_count,
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
  test::TaskEnvironment task_environment_;
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
  EXPECT_EQ(frame->format(), std::nullopt);
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
  VerifyAllocationSize(0, std::nullopt, std::nullopt, false, kTotalSizeInBytes,
                       "Default");
  VerifyAllocationSize(1, std::nullopt, std::nullopt, false, kTotalSizeInBytes,
                       "Valid index.");
  VerifyAllocationSize(0, 0, kFrames, false, kTotalSizeInBytes,
                       "Specifying defaults");

  // Cases where we cover a subset of samples.
  VerifyAllocationSize(0, kFrames / 2, std::nullopt, false,
                       kTotalSizeInBytes / 2, "Valid offset, no count");
  VerifyAllocationSize(0, kFrames / 2, kFrames / 4, false,
                       kTotalSizeInBytes / 4, "Valid offset and count");
  VerifyAllocationSize(0, std::nullopt, kFrames / 2, false,
                       kTotalSizeInBytes / 2, "No offset, valid count");

  // Copying 0 frames is technically valid.
  VerifyAllocationSize(0, std::nullopt, 0, false, 0, "Frame count is 0");

  // Failures
  VerifyAllocationSize(2, std::nullopt, std::nullopt, true, 0,
                       "Invalid index.");
  VerifyAllocationSize(0, kFrames, std::nullopt, true, 0, "Offset too big");
  VerifyAllocationSize(0, std::nullopt, kFrames + 1, true, 0, "Count too big");
  VerifyAllocationSize(0, 1, kFrames, true, 0, "Count too big, with offset");
}

TEST_F(AudioDataTest, CopyTo_DestinationTooSmall) {
  V8TestingScope scope;
  auto* frame =
      CreateDefaultAudioData(scope.GetScriptState(), scope.GetExceptionState());
  auto* options = CreateCopyToOptions(/*index=*/0, /*offset=*/std::nullopt,
                                      /*count=*/std::nullopt);

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
  auto* options = CreateCopyToOptions(/*index=*/0, /*offset=*/std::nullopt,
                                      /*count=*/std::nullopt);

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
  auto* options = CreateCopyToOptions(/*index=*/1, /*offset=*/std::nullopt,
                                      /*count=*/std::nullopt);

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

// Check that sample-aligned ArrayBuffers can be transferred to AudioData
TEST_F(AudioDataTest, TransferBuffer) {
  V8TestingScope scope;
  std::string data = "audio data";
  auto* buffer = DOMArrayBuffer::Create(base::as_byte_span(data));
  auto* buffer_source = MakeGarbageCollected<AllowSharedBufferSource>(buffer);
  const void* buffer_data_ptr = buffer->Data();

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

  auto* audio_data = MakeGarbageCollected<AudioData>(
      scope.GetScriptState(), audio_data_init, scope.GetExceptionState());

  EXPECT_EQ(audio_data->format(), "u8");
  EXPECT_EQ(audio_data->numberOfFrames(), data.size());
  EXPECT_EQ(audio_data->numberOfChannels(), 1u);

  EXPECT_EQ(audio_data->data()->channel_data()[0], buffer_data_ptr);
  auto* options = CreateCopyToOptions(0, 0, {});
  uint32_t allocations_size =
      audio_data->allocationSize(options, scope.GetExceptionState());

  EXPECT_TRUE(buffer->IsDetached());
  EXPECT_EQ(allocations_size, data.size());
}

// Check that not-sample-aligned ArrayBuffers are copied AudioData
TEST_F(AudioDataTest, FailToTransferUnAlignedBuffer) {
  V8TestingScope scope;
  const uint32_t frames = 3;
  std::vector<float> data{0.0, 1.0, 2.0, 3.0, 4.0};
  auto* buffer = DOMArrayBuffer::Create(base::as_byte_span(data));
  auto* view = DOMDataView::Create(
      buffer, 1 /* offset one byte from the float ptr, that how we are sure that
                   the view is not aligned to sizeof(float) */
      ,
      frames * sizeof(float));
  auto* buffer_source = MakeGarbageCollected<AllowSharedBufferSource>(
      MaybeShared<DOMArrayBufferView>(view));

  MakeGarbageCollected<AllowSharedBufferSource>(buffer);
  const void* buffer_data_ptr = buffer->Data();

  auto* audio_data_init = AudioDataInit::Create();
  audio_data_init->setData(buffer_source);
  audio_data_init->setTimestamp(0);
  audio_data_init->setNumberOfChannels(1);
  audio_data_init->setNumberOfFrames(frames);
  audio_data_init->setSampleRate(kSampleRate);
  audio_data_init->setFormat("f32");
  HeapVector<Member<DOMArrayBuffer>> transfer;
  transfer.push_back(Member<DOMArrayBuffer>(buffer));
  audio_data_init->setTransfer(std::move(transfer));

  auto* audio_data = MakeGarbageCollected<AudioData>(
      scope.GetScriptState(), audio_data_init, scope.GetExceptionState());

  // Making sure that the data was copied, not just aliased.
  EXPECT_NE(audio_data->data()->channel_data()[0], buffer_data_ptr);
  EXPECT_EQ(audio_data->numberOfFrames(), frames);
  auto* options = CreateCopyToOptions(0, 0, {});
  uint32_t allocations_size =
      audio_data->allocationSize(options, scope.GetExceptionState());

  // Even though we copied the data, the buffer still needs to be aligned.
  EXPECT_TRUE(buffer->IsDetached());
  EXPECT_EQ(allocations_size, frames * sizeof(float));
}

TEST_F(AudioDataTest, CopyTo_Offset) {
  V8TestingScope scope;

  auto* frame =
      CreateDefaultAudioData(scope.GetScriptState(), scope.GetExceptionState());
  auto* options =
      CreateCopyToOptions(/*index=*/0, kOffset, /*count=*/std::nullopt);

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
  auto* options = CreateCopyToOptions(/*index=*/0, /*offset=*/std::nullopt,
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

  auto* options = CreateCopyToOptions(/*index=*/1, kOffset, kPartialFrameCount);

  // Verify that plane indexes > 1 throw, for interleaved formats.
  {
    DummyExceptionStateForTesting exception_state;
    frame->allocationSize(options, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  // Verify that copy conversion to a planar format supports indexes > 1,
  // even if the source is interleaved.
  options->setFormat(V8AudioSampleFormat::Enum::kF32Planar);
  int allocations_size =
      frame->allocationSize(options, scope.GetExceptionState());
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

struct U8Traits {
  static constexpr std::string Format = "u8";
  static constexpr std::string PlanarFormat = "u8-planar";
  using Traits = media::UnsignedInt8SampleTypeTraits;
};

struct S16Traits {
  static constexpr std::string Format = "s16";
  static constexpr std::string PlanarFormat = "s16-planar";
  using Traits = media::SignedInt16SampleTypeTraits;
};

struct S32Traits {
  static constexpr std::string Format = "s32";
  static constexpr std::string PlanarFormat = "s32-planar";
  using Traits = media::SignedInt32SampleTypeTraits;
};

struct F32Traits {
  static constexpr std::string Format = "f32";
  static constexpr std::string PlanarFormat = "f32-planar";
  using Traits = media::Float32SampleTypeTraits;
};

template <typename SourceTraits, typename TargetTraits>
struct ConversionConfig {
  using From = SourceTraits;
  using To = TargetTraits;
  static std::string config_name() {
    return From::Format + "_to_" + To::Format;
  }
};

template <typename TestConfig>
class AudioDataConversionTest : public testing::Test {
 protected:
  AudioDataInit* CreateAudioDataInit(AllowSharedBufferSource* data,
                                     std::string format) {
    auto* audio_data_init = AudioDataInit::Create();
    audio_data_init->setData(data);
    audio_data_init->setTimestamp(kTimestampInMicroSeconds);
    audio_data_init->setNumberOfChannels(kChannels);
    audio_data_init->setNumberOfFrames(kFrames);
    audio_data_init->setSampleRate(kSampleRate);
    audio_data_init->setFormat(String(format));
    return audio_data_init;
  }

  // Creates test AudioData with kMinValue in channel 0 and kMaxValue in
  // channel 1. If `use_offset`, the first sample of every channel will be
  // kZeroPointValue; if `use_frame_count`, the last sample of every channel
  // will be kZeroPointValue. This allows us to verify that we respect bounds
  // when copying.
  AudioData* CreateAudioData(std::string format,
                             bool planar,
                             bool use_offset,
                             bool use_frame_count,
                             ScriptState* script_state,
                             ExceptionState& exception_state) {
    auto* data = planar ? CreatePlanarData(use_offset, use_frame_count)
                        : CreateInterleavedData(use_offset, use_frame_count);
    auto* audio_data_init = CreateAudioDataInit(data, format);
    return MakeGarbageCollected<AudioData>(script_state, audio_data_init,
                                           exception_state);
  }

  // Creates CopyToOptions. If `use_offset` is true, we exclude the first
  // sample. If `use_frame_count`, we exclude the last sample.
  AudioDataCopyToOptions* CreateCopyToOptions(int plane_index,
                                              std::string format,
                                              bool use_offset,
                                              bool use_frame_count) {
    auto* copy_to_options = AudioDataCopyToOptions::Create();
    copy_to_options->setPlaneIndex(plane_index);
    copy_to_options->setFormat(String(format));
    int total_frames = kFrames;

    if (use_offset) {
      copy_to_options->setFrameOffset(1);
      --total_frames;
    }

    if (use_frame_count) {
      copy_to_options->setFrameCount(total_frames - 1);
    }

    return copy_to_options;
  }

  // Returns planar data with the source sample type's min value in
  // channel 0, and its max value in channel 1.
  // If `use_offset` is true, the first sample of every channel will be 0.
  // If `use_frame_count` is true, the last sample of every channel will be 0.
  AllowSharedBufferSource* CreatePlanarData(bool use_offset,
                                            bool use_frame_count) {
    static_assert(kChannels == 2, "CreatePlanarData() assumes 2 channels");
    using SourceTraits = TestConfig::From::Traits;
    using ValueType = SourceTraits::ValueType;
    auto* buffer =
        DOMArrayBuffer::Create(kChannels * kFrames, sizeof(ValueType));

    ValueType* plane_start = reinterpret_cast<ValueType*>(buffer->Data());
    for (int i = 0; i < kFrames; ++i) {
      plane_start[i] = SourceTraits::kMinValue;
    }

    if (use_offset) {
      plane_start[0] = SourceTraits::kZeroPointValue;
    }
    if (use_frame_count) {
      plane_start[kFrames - 1] = SourceTraits::kZeroPointValue;
    }

    plane_start += kFrames;

    for (int i = 0; i < kFrames; ++i) {
      plane_start[i] = SourceTraits::kMaxValue;
    }

    if (use_offset) {
      plane_start[0] = SourceTraits::kZeroPointValue;
    }
    if (use_frame_count) {
      plane_start[kFrames - 1] = SourceTraits::kZeroPointValue;
    }

    return MakeGarbageCollected<AllowSharedBufferSource>(buffer);
  }

  // Returns interleaved data the source sample type's min value in channel 0,
  // and its max value in channel 1.
  // If `use_offset` is true, the first sample of every channel will be 0.
  // If `use_frame_count` is true, the last sample of every channel will be 0.
  AllowSharedBufferSource* CreateInterleavedData(bool use_offset,
                                                 bool use_frame_count) {
    static_assert(kChannels == 2,
                  "CreateInterleavedData() assumes 2 channels.");
    using SourceTraits = TestConfig::From::Traits;
    using ValueType = SourceTraits::ValueType;
    constexpr int kTotalSamples = kChannels * kFrames;
    auto* buffer = DOMArrayBuffer::Create(kTotalSamples, sizeof(ValueType));

    ValueType* plane_start = reinterpret_cast<ValueType*>(buffer->Data());
    for (int i = 0; i < kTotalSamples; i += 2) {
      plane_start[i] = SourceTraits::kMinValue;
      plane_start[i + 1] = SourceTraits::kMaxValue;
    }

    if (use_offset) {
      plane_start[0] = SourceTraits::kZeroPointValue;
      plane_start[1] = SourceTraits::kZeroPointValue;
    }

    if (use_frame_count) {
      plane_start[kTotalSamples - 2] = SourceTraits::kZeroPointValue;
      plane_start[kTotalSamples - 1] = SourceTraits::kZeroPointValue;
    }

    return MakeGarbageCollected<AllowSharedBufferSource>(buffer);
  }

  int GetFramesToCopy(bool use_offset, bool use_frame_count) {
    int frames_to_copy = kFrames;
    if (use_offset) {
      --frames_to_copy;
    }
    if (use_frame_count) {
      --frames_to_copy;
    }
    return frames_to_copy;
  }

  void TestConversionToPlanar(bool source_is_planar,
                              bool use_offset,
                              bool use_frame_count) {
    using Config = TestConfig;
    using TargetType = Config::To::Traits::ValueType;
    constexpr int kChannelToCopy = 1;

    std::string source_format =
        source_is_planar ? Config::From::PlanarFormat : Config::From::Format;

    // Create original data. The first and last frame will be zero'ed, if
    // `use_offset` or `use_frame_count` are set, respectively.
    V8TestingScope scope;
    auto* audio_data = CreateAudioData(
        source_format, source_is_planar, use_offset, use_frame_count,
        scope.GetScriptState(), scope.GetExceptionState());
    ASSERT_FALSE(scope.GetExceptionState().HadException())
        << scope.GetExceptionState().Message();

    // Prepare to copy.
    auto* copy_to_options = CreateCopyToOptions(
        kChannelToCopy, Config::To::PlanarFormat, use_offset, use_frame_count);

    const int frames_to_copy = GetFramesToCopy(use_offset, use_frame_count);
    DOMArrayBuffer* data_copy =
        DOMArrayBuffer::Create(frames_to_copy, sizeof(TargetType));
    AllowSharedBufferSource* dest =
        MakeGarbageCollected<AllowSharedBufferSource>(data_copy);

    // Copy frames, potentially excluding the first and last frame if
    // `use_offset` or `use_frame_count` are set, respectively.
    audio_data->copyTo(dest, copy_to_options, scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException())
        << scope.GetExceptionState().Message();

    TargetType* copied_data = static_cast<TargetType*>(data_copy->Data());

    // `kChannelToCopy` should only contain kMaxValue
    for (int i = 0; i < frames_to_copy; ++i) {
      ASSERT_EQ(copied_data[i], Config::To::Traits::kMaxValue);
    }
  }

  void TestConversionToInterleaved(bool source_is_planar,
                                   bool use_offset,
                                   bool use_frame_count) {
    using Config = TestConfig;
    using TargetType = Config::To::Traits::ValueType;

    std::string source_format =
        source_is_planar ? Config::From::PlanarFormat : Config::From::Format;

    // Create original data. The first and last frame will be zero'ed, if
    // `use_offset` or `use_frame_count` are set, respectively.
    V8TestingScope scope;
    auto* audio_data = CreateAudioData(
        source_format, source_is_planar, use_offset, use_frame_count,
        scope.GetScriptState(), scope.GetExceptionState());
    ASSERT_FALSE(scope.GetExceptionState().HadException())
        << scope.GetExceptionState().Message();

    // Prepare to copy.
    auto* copy_to_options =
        CreateCopyToOptions(0, Config::To::Format, use_offset, use_frame_count);

    const int total_frames =
        GetFramesToCopy(use_offset, use_frame_count) * kChannels;
    DOMArrayBuffer* data_copy =
        DOMArrayBuffer::Create(total_frames, sizeof(TargetType));
    AllowSharedBufferSource* dest =
        MakeGarbageCollected<AllowSharedBufferSource>(data_copy);

    // Copy frames, potentially excluding the first and last frame if
    // `use_offset` or `use_frame_count` are set, respectively.
    audio_data->copyTo(dest, copy_to_options, scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException())
        << scope.GetExceptionState().Message();

    TargetType* copied_data = static_cast<TargetType*>(data_copy->Data());

    // The interleaved data should have kMinValue in
    // channel 0 and kMaxValue in channel 1.
    for (int i = 0; i < total_frames; i += 2) {
      ASSERT_EQ(copied_data[i], Config::To::Traits::kMinValue);
      ASSERT_EQ(copied_data[i + 1], Config::To::Traits::kMaxValue);
    }
  }

  test::TaskEnvironment task_environment_;
};

TYPED_TEST_SUITE_P(AudioDataConversionTest);

TYPED_TEST_P(AudioDataConversionTest, PlanarToPlanar) {
  const bool source_is_planar = true;

  {
    SCOPED_TRACE(TypeParam::config_name() + "_all_frames");
    this->TestConversionToPlanar(source_is_planar, false, false);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_offset");
    this->TestConversionToPlanar(source_is_planar, true, false);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_frame_count");
    this->TestConversionToPlanar(source_is_planar, false, true);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_offset_and_frame_count");
    this->TestConversionToPlanar(source_is_planar, true, true);
  }
}

TYPED_TEST_P(AudioDataConversionTest, InterleavedToPlanar) {
  const bool source_is_planar = false;

  {
    SCOPED_TRACE(TypeParam::config_name() + "_all_frames");
    this->TestConversionToPlanar(source_is_planar, false, false);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_offset");
    this->TestConversionToPlanar(source_is_planar, true, false);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_frame_count");
    this->TestConversionToPlanar(source_is_planar, false, true);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_offset_and_frame_count");
    this->TestConversionToPlanar(source_is_planar, true, true);
  }
}

TYPED_TEST_P(AudioDataConversionTest, PlanarToInterleaved) {
  const bool source_is_planar = true;

  {
    SCOPED_TRACE(TypeParam::config_name() + "_all_frames");
    this->TestConversionToInterleaved(source_is_planar, false, false);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_offset");
    this->TestConversionToInterleaved(source_is_planar, true, false);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_frame_count");
    this->TestConversionToInterleaved(source_is_planar, false, true);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_offset_and_frame_count");
    this->TestConversionToInterleaved(source_is_planar, true, true);
  }
}

TYPED_TEST_P(AudioDataConversionTest, InterleavedToInterleaved) {
  const bool source_is_planar = false;

  {
    SCOPED_TRACE(TypeParam::config_name() + "_all_frames");
    this->TestConversionToInterleaved(source_is_planar, false, false);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_offset");
    this->TestConversionToInterleaved(source_is_planar, true, false);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_frame_count");
    this->TestConversionToInterleaved(source_is_planar, false, true);
  }

  {
    SCOPED_TRACE(TypeParam::config_name() + "_with_offset_and_frame_count");
    this->TestConversionToInterleaved(source_is_planar, true, true);
  }
}

REGISTER_TYPED_TEST_SUITE_P(AudioDataConversionTest,
                            PlanarToPlanar,
                            InterleavedToPlanar,
                            PlanarToInterleaved,
                            InterleavedToInterleaved);

typedef ::testing::Types<ConversionConfig<U8Traits, U8Traits>,
                         ConversionConfig<U8Traits, S16Traits>,
                         ConversionConfig<U8Traits, S32Traits>,
                         ConversionConfig<U8Traits, F32Traits>,
                         ConversionConfig<S16Traits, U8Traits>,
                         ConversionConfig<S16Traits, S16Traits>,
                         ConversionConfig<S16Traits, S32Traits>,
                         ConversionConfig<S16Traits, F32Traits>,
                         ConversionConfig<S32Traits, U8Traits>,
                         ConversionConfig<S32Traits, S16Traits>,
                         ConversionConfig<S32Traits, S32Traits>,
                         ConversionConfig<S32Traits, F32Traits>,
                         ConversionConfig<F32Traits, U8Traits>,
                         ConversionConfig<F32Traits, S16Traits>,
                         ConversionConfig<F32Traits, S32Traits>,
                         ConversionConfig<F32Traits, F32Traits>>
    TestConfigs;

INSTANTIATE_TYPED_TEST_SUITE_P(CommonTypes,
                               AudioDataConversionTest,
                               TestConfigs);

}  // namespace blink
