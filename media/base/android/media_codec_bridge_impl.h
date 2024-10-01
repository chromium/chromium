// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_IMPL_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_codec_direction.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

class VideoColorSpace;

// Configuration info for MediaCodec.
class MEDIA_EXPORT VideoCodecConfig {
 public:
  VideoCodecConfig();

  VideoCodecConfig(const VideoCodecConfig&) = delete;
  VideoCodecConfig& operator=(const VideoCodecConfig&) = delete;

  ~VideoCodecConfig();

  VideoCodec codec = VideoCodec::kUnknown;

  CodecType codec_type = CodecType::kAny;

  // The initial coded size. The actual size might change at any time, so this
  // is only a hint.
  gfx::Size initial_expected_coded_size;

  // The surface that MediaCodec is configured to output to.
  base::android::ScopedJavaGlobalRef<jobject> surface;

  // The MediaCrypto that MediaCodec is configured with for an encrypted stream.
  base::android::ScopedJavaGlobalRef<jobject> media_crypto;

  // Codec specific data (SPS and PPS for H264). See MediaCodec docs.
  std::vector<uint8_t> csd0;
  std::vector<uint8_t> csd1;

  VideoColorSpace container_color_space;

  // VP9 HDR metadata is only embedded in the container. HDR10 metadata is
  // embedded in the video stream.
  std::optional<gfx::HDRMetadata> hdr_metadata;

  // Enables the async MediaCodec.Callback API. |on_buffers_available_cb|
  // will be called when input or output buffers are available. This will be
  // called on an arbitrary thread, so use base::BindPostTaskToCurrentDefault if
  // needed.
  base::RepeatingClosure on_buffers_available_cb;

  // The name of decoder/encoder. It's used to create the MediaCodec instance.
  std::string name;

  // Enables Block Model (LinearBlock).
  bool use_block_model = false;

  // The profile of decoder.
  VideoCodecProfile profile;
};

// A bridge to a Java MediaCodec.
class MEDIA_EXPORT MediaCodecBridgeImpl : public MediaCodecBridge {
 public:
  // Creates and starts a new MediaCodec configured for decoding. Returns
  // nullptr on failure.
  static std::unique_ptr<MediaCodecBridge> CreateVideoDecoder(
      const VideoCodecConfig& config);

  // Creates and starts a new MediaCodec configured for encoding. Returns
  // nullptr on failure.
  static std::unique_ptr<MediaCodecBridge> CreateVideoEncoder(
      VideoCodec codec,       // e.g. media::VideoCodec::kVP8
      const gfx::Size& size,  // input frame size
      int bit_rate,           // bits/second
      int frame_rate,         // frames/second
      int i_frame_interval,   // count
      int color_format);      // MediaCodecInfo.CodecCapabilities.

  // Creates and starts a new MediaCodec configured for decoding. Returns
  // nullptr on failure.
  static std::unique_ptr<MediaCodecBridge> CreateAudioDecoder(
      const AudioDecoderConfig& config,
      const base::android::JavaRef<jobject>& media_crypto,
      // Enables the async MediaCodec.Callback API. |on_buffers_available_cb|
      // will be called when input or output buffers are available. This will be
      // called on an arbitrary thread, so use
      // base::BindPostTaskToCurrentDefault if needed.
      //
      // May only be used on API level 23 and higher.
      base::RepeatingClosure on_buffers_available_cb =
          base::RepeatingClosure());

  // Required for tests that wish to use a |on_buffers_available_cb| when
  // creating a MediaCodec. Does nothing unless on API level 23+.
  static void SetupCallbackHandlerForTesting();

  MediaCodecBridgeImpl(const MediaCodecBridgeImpl&) = delete;
  MediaCodecBridgeImpl& operator=(const MediaCodecBridgeImpl&) = delete;

  ~MediaCodecBridgeImpl() override;

  // MediaCodecBridge implementation.
  void Stop() override;
  MediaCodecResult Flush() override;
  MediaCodecResult GetOutputSize(gfx::Size* size) override;
  MediaCodecResult GetOutputSamplingRate(int* sampling_rate) override;
  MediaCodecResult GetOutputChannelCount(int* channel_count) override;
  MediaCodecResult GetOutputColorSpace(gfx::ColorSpace* color_space) override;
  MediaCodecResult GetInputFormat(int* stride,
                                  int* slice_height,
                                  gfx::Size* encoded_size) override;
  MediaCodecResult QueueInputBuffer(int index,
                                    const uint8_t* data,
                                    size_t data_size,
                                    base::TimeDelta presentation_time) override;
  MediaCodecResult QueueInputBlock(int index,
                                   base::span<const uint8_t> data,
                                   base::TimeDelta presentation_time,
                                   bool is_eos) override;
  MediaCodecResult QueueSecureInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const std::string& key_id,
      const std::string& iv,
      const std::vector<SubsampleEntry>& subsamples,
      EncryptionScheme encryption_scheme,
      std::optional<EncryptionPattern> encryption_pattern,
      base::TimeDelta presentation_time) override;
  void QueueEOS(int input_buffer_index) override;
  MediaCodecResult DequeueInputBuffer(base::TimeDelta timeout,
                                      int* index) override;
  MediaCodecResult DequeueOutputBuffer(base::TimeDelta timeout,
                                       int* index,
                                       size_t* offset,
                                       size_t* size,
                                       base::TimeDelta* presentation_time,
                                       bool* end_of_stream,
                                       bool* key_frame) override;

  void ReleaseOutputBuffer(int index, bool render) override;
  MediaCodecResult GetInputBuffer(int input_buffer_index,
                                  uint8_t** data,
                                  size_t* capacity) override;
  MediaCodecResult CopyFromOutputBuffer(int index,
                                        size_t offset,
                                        void* dst,
                                        size_t num) override;
  std::string GetName() override;
  bool IsSoftwareCodec() override;
  bool SetSurface(const base::android::JavaRef<jobject>& surface) override;
  void SetVideoBitrate(int bps, int frame_rate) override;
  void RequestKeyFrameSoon() override;
  CodecType GetCodecType() const override;
  size_t GetMaxInputSize() override;

 private:
  MediaCodecBridgeImpl(CodecType codec_type,
                       base::android::ScopedJavaGlobalRef<jobject> j_bridge,
                       base::RepeatingClosure on_buffers_available_cb =
                           base::RepeatingClosure());

  // Fills the given input buffer. Returns false if |data_size| exceeds the
  // input buffer's capacity (and doesn't touch the input buffer in that case).
  [[nodiscard]] bool FillInputBuffer(int index,
                                     const uint8_t* data,
                                     size_t data_size);

  // Gets the address of the data in the given output buffer given by |index|
  // and |offset|. The number of bytes available to read is written to
  // |*capacity| and the address is written to |*addr|. Returns
  // kError if an error occurs, or kOk otherwise.
  MediaCodecResult GetOutputBufferAddress(int index,
                                          size_t offset,
                                          const uint8_t** addr,
                                          size_t* capacity);

  void OnBuffersAvailable(
      JNIEnv* /* env */,
      const base::android::JavaParamRef<jobject>& /* obj */) override;

  const CodecType codec_type_;

  base::RepeatingClosure on_buffers_available_cb_;

  // The Java MediaCodecBridge instance.
  base::android::ScopedJavaGlobalRef<jobject> j_bridge_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_IMPL_H_
