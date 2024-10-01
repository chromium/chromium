// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MOCK_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MOCK_MEDIA_CODEC_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/test_destruction_observable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockMediaCodecBridge : public MediaCodecBridge,
                             public DestructionObservable {
 public:
  MockMediaCodecBridge();

  MockMediaCodecBridge(const MockMediaCodecBridge&) = delete;
  MockMediaCodecBridge& operator=(const MockMediaCodecBridge&) = delete;

  ~MockMediaCodecBridge() override;

  // Helpers for conveniently setting expectations.
  enum IsEos { kEos, kNotEos };
  void AcceptOneInput(IsEos eos = kNotEos);
  void ProduceOneOutput(IsEos eos = kNotEos);

  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Flush, MediaCodecResult());
  MOCK_METHOD1(GetOutputSize, MediaCodecResult(gfx::Size* size));
  MOCK_METHOD1(GetOutputSamplingRate, MediaCodecResult(int* sampling_rate));
  MOCK_METHOD1(GetOutputChannelCount, MediaCodecResult(int* channel_count));
  MOCK_METHOD1(GetOutputColorSpace,
               MediaCodecResult(gfx::ColorSpace* color_space));
  MOCK_METHOD3(GetInputFormat,
               MediaCodecResult(int* stride,
                                int* slice_height,
                                gfx::Size* encoded_size));
  MOCK_METHOD4(QueueInputBuffer,
               MediaCodecResult(int index,
                                const uint8_t* data,
                                size_t data_size,
                                base::TimeDelta presentation_time));
  MOCK_METHOD4(QueueInputBlock,
               MediaCodecResult(int index,
                                base::span<const uint8_t> data,
                                base::TimeDelta presentation_time,
                                bool is_eos));
  MOCK_METHOD9(
      QueueSecureInputBuffer,
      MediaCodecResult(int index,
                       const uint8_t* data,
                       size_t data_size,
                       const std::string& key_id,
                       const std::string& iv,
                       const std::vector<SubsampleEntry>& subsamples,
                       EncryptionScheme encryption_scheme,
                       std::optional<EncryptionPattern> encryption_pattern,
                       base::TimeDelta presentation_time));
  MOCK_METHOD1(QueueEOS, void(int input_buffer_index));
  MOCK_METHOD2(DequeueInputBuffer,
               MediaCodecResult(base::TimeDelta timeout, int* index));
  MOCK_METHOD7(DequeueOutputBuffer,
               MediaCodecResult(base::TimeDelta timeout,
                                int* index,
                                size_t* offset,
                                size_t* size,
                                base::TimeDelta* presentation_time,
                                bool* end_of_stream,
                                bool* key_frame));
  MOCK_METHOD2(ReleaseOutputBuffer, void(int index, bool render));
  MOCK_METHOD3(GetInputBuffer,
               MediaCodecResult(int input_buffer_index,
                                uint8_t** data,
                                size_t* capacity));
  MOCK_METHOD4(
      CopyFromOutputBuffer,
      MediaCodecResult(int index, size_t offset, void* dst, size_t num));
  std::string GetName() override;
  bool IsSoftwareCodec() override;
  MOCK_METHOD1(SetSurface,
               bool(const base::android::JavaRef<jobject>& surface));
  MOCK_METHOD2(SetVideoBitrate, void(int bps, int frame_rate));
  MOCK_METHOD0(RequestKeyFrameSoon, void());
  MOCK_METHOD0(IsAdaptivePlaybackSupported, bool());
  MOCK_METHOD2(OnBuffersAvailable,
               void(JNIEnv*, const base::android::JavaParamRef<jobject>&));
  MOCK_METHOD0(GetMaxInputSize, size_t());
  CodecType GetCodecType() const override;

  // Return true if the codec is already drained.
  bool IsDrained() const;

  static std::unique_ptr<MediaCodecBridge> CreateVideoDecoder(
      const VideoCodecConfig& config);
  static std::unique_ptr<MockMediaCodecBridge> CreateMockVideoDecoder(
      const VideoCodecConfig& config);

 private:
  // Is the codec in the drained state?
  bool is_drained_ = true;

  CodecType codec_type_ = CodecType::kAny;
  std::string name_;
  bool is_software_codec_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MOCK_MEDIA_CODEC_BRIDGE_H_
