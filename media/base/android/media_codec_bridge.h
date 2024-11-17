// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "media/base/encryption_pattern.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_export.h"
#include "media/base/status.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

struct SubsampleEntry;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
enum class CodecType {
  kAny,
  kSecure,    // Note that all secure codecs are HW codecs.
  kSoftware,  // In some cases hardware codecs could hang the GPU process.
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
// GENERATED_JAVA_PREFIX_TO_STRIP: MEDIA_CODEC_
// These enums are also reported to UMA so values should not be renumbered or
// reused.
enum MediaCodecStatus {
  MEDIA_CODEC_OK = 0,
  MEDIA_CODEC_TRY_AGAIN_LATER = 1,
  MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED = 2,
  MEDIA_CODEC_OUTPUT_FORMAT_CHANGED = 3,
  MEDIA_CODEC_NO_KEY = 4,
  MEDIA_CODEC_ERROR = 5,
  MEDIA_CODEC_KEY_EXPIRED = 6,
  MEDIA_CODEC_RESOURCE_BUSY = 7,
  MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION = 8,
  MEDIA_CODEC_SESSION_NOT_OPENED = 9,
  MEDIA_CODEC_UNSUPPORTED_OPERATION = 10,
  MEDIA_CODEC_INSUFFICIENT_SECURITY = 11,
  MEDIA_CODEC_FRAME_TOO_LARGE = 12,
  MEDIA_CODEC_LOST_STATE = 13,
  MEDIA_CODEC_GENERIC_OEM = 14,
  MEDIA_CODEC_GENERIC_PLUGIN = 15,
  MEDIA_CODEC_LICENSE_PARSE = 16,
  MEDIA_CODEC_MEDIA_FRAMEWORK = 17,
  MEDIA_CODEC_ZERO_SUBSAMPLES = 18,
  MEDIA_CODEC_UNKNOWN_CIPHER_MODE = 19,
  MEDIA_CODEC_PATTERN_ENCRYPTION_NOT_SUPPORTED = 20,
  MEDIA_CODEC_MAX = MEDIA_CODEC_PATTERN_ENCRYPTION_NOT_SUPPORTED,
};

struct MediaCodecResultTraits {
  enum class Codes : StatusCodeType {
    kOk,
    kTryAgainLater,
    kOutputBuffersChanged,
    kOutputFormatChanged,
    kNoKey,
    kError,
  };
  static constexpr StatusGroupType Group() { return "MediaCodecResult"; }
};
using MediaCodecResult = TypedStatus<MediaCodecResultTraits>;

// An interface for a bridge to an Android MediaCodec.
class MEDIA_EXPORT MediaCodecBridge {
 public:
  MediaCodecBridge() = default;

  MediaCodecBridge(const MediaCodecBridge&) = delete;
  MediaCodecBridge& operator=(const MediaCodecBridge&) = delete;

  virtual ~MediaCodecBridge() = default;

  // Calls MediaCodec#stop(). However, due to buggy implementations (b/8125974)
  // Stop() followed by Start() may not work on some devices. For reliability,
  // it's recommended to delete the instance and create a new one instead.
  virtual void Stop() = 0;

  // Calls MediaCodec#flush(). The codec takes ownership of all input and output
  // buffers previously dequeued when this is called. Returns kError
  // if an unexpected error happens, or kOk otherwise.
  virtual MediaCodecResult Flush() = 0;

  // Returns the output size. This is valid after DequeueOutputBuffer()
  // signals a format change by returning OUTPUT_FORMAT_CHANGED.
  // Returns kError if an error occurs, or kOk otherwise.
  virtual MediaCodecResult GetOutputSize(gfx::Size* size) = 0;

  // Gets the sampling rate. This is valid after DequeueOutputBuffer()
  // signals a format change by returning kOutputFormatChanged.
  // Returns kError if an error occurs, or kOk otherwise.
  virtual MediaCodecResult GetOutputSamplingRate(int* sampling_rate) = 0;

  // Fills |channel_count| with the number of audio channels.  This is valid
  // after DequeueOutputBuffer() signals a format change by returning
  // kOutputFormatChanged. Returns kError if an error occurs,
  // or kOk otherwise.
  virtual MediaCodecResult GetOutputChannelCount(int* channel_count) = 0;

  // Fills in |color_space| with the color space of the decoded video.  This
  // is valid after DequeueOutputBuffer() signals a format change.  Will return
  // kOk on success, with |color_space| initialized, or
  // kError with |color_space| unmodified otherwise.
  virtual MediaCodecResult GetOutputColorSpace(
      gfx::ColorSpace* color_space) = 0;

  // Fills in |stride| with required Y-plane stride in the encoder's input
  // buffer. Returns kOk on success, with |stride| initialized, or
  // kError with |stride| unmodified otherwise.
  // Fills in |slice_height| with required Y-plane height in the encoder's input
  // buffer. (i.e. the number of rows that must be skipped to get from the top
  // of the Y plane to the top of the UV plane in the bytebuffer.)
  // Fills in |encoded_size| with actual size the encoder was configured for,
  // which may differ if the codec requires 16x16 aligned resolutions.
  // (see MediaFormat#KEY_STRIDE for more details)
  virtual MediaCodecResult GetInputFormat(int* stride,
                                          int* slice_height,
                                          gfx::Size* encoded_size) = 0;

  // Submits a byte array to the given input buffer. Call this after getting an
  // available buffer from DequeueInputBuffer(). `data` will be copied into the
  // input buffer.
  virtual MediaCodecResult QueueInputBuffer(
      int index,
      base::span<const uint8_t> data,
      base::TimeDelta presentation_time) = 0;
  // Similar to QueueInputBuffer() but submits the input buffer referenced by
  // `index` assuming it has already been filled.
  virtual MediaCodecResult QueueFilledInputBuffer(
      int index,
      size_t data_size,
      base::TimeDelta presentation_time) = 0;

  // Submits a byte array to the given input buffer, using LinearBlock.
  virtual MediaCodecResult QueueInputBlock(int index,
                                           base::span<const uint8_t> data,
                                           base::TimeDelta presentation_time,
                                           bool is_eos) = 0;

  // As above but for encrypted buffers. NULL |subsamples| indicates the
  // whole buffer is encrypted.
  virtual MediaCodecResult QueueSecureInputBuffer(
      int index,
      base::span<const uint8_t> data,
      const std::string& key_id,
      const std::string& iv,
      const std::vector<SubsampleEntry>& subsamples,
      EncryptionScheme encryption_scheme,
      std::optional<EncryptionPattern> encryption_pattern,
      base::TimeDelta presentation_time) = 0;

  // Submits an empty buffer with the END_OF_STREAM flag set.
  virtual void QueueEOS(int input_buffer_index) = 0;

  // Returns:
  // kOk if an input buffer is ready to be filled with valid data,
  // kTryAgainLater if no such buffer is available, or
  // kError if unexpected error happens.
  virtual MediaCodecResult DequeueInputBuffer(base::TimeDelta timeout,
                                              int* index) = 0;

  // Dequeues an output buffer, block for up to |timeout|.
  // Returns the status of this operation. If OK is returned, the output
  // parameters should be populated. Otherwise, the values of output parameters
  // should not be used.  Output parameters other than index/offset/size are
  // optional and only set if not NULL.
  virtual MediaCodecResult DequeueOutputBuffer(
      base::TimeDelta timeout,
      int* index,
      size_t* offset,
      size_t* size,
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      bool* key_frame) = 0;

  // Returns the buffer to the codec. If you previously specified a surface when
  // configuring this video decoder you can optionally render the buffer.
  virtual void ReleaseOutputBuffer(int index, bool render) = 0;

  // Returns an input buffer's base pointer and capacity.
  virtual base::span<uint8_t> GetInputBuffer(int input_buffer_index) = 0;

  // Copies |num| bytes from output buffer |index|'s |offset| into the memory
  // region pointed to by |dst|. Returns kError if an error occurs, or kOk
  // otherwise.
  virtual MediaCodecResult CopyFromOutputBuffer(int index,
                                                size_t offset,
                                                base::span<uint8_t> dst) = 0;

  // Gets the component name. Before API level 18 this returns an empty string.
  virtual std::string GetName() = 0;

  // Returns whether the media codec implementation is software codec.
  virtual bool IsSoftwareCodec() = 0;

  // Changes the output surface for the MediaCodec. May only be used on API
  // level 23 and higher (Marshmallow).
  virtual bool SetSurface(const base::android::JavaRef<jobject>& surface) = 0;

  // Sets the video encoder target bitrate and framerate.
  virtual void SetVideoBitrate(int bps, int frame_rate) = 0;

  // Requests that the video encoder insert a key frame.
  virtual void RequestKeyFrameSoon() = 0;

  // When the MediaCodec has been configured in async mode, this is called when
  // input or output buffers are available.
  virtual void OnBuffersAvailable(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) = 0;

  // Returns the CodecType this codec was created with.
  virtual CodecType GetCodecType() const = 0;

  // Returns the max input size we configured the codec with.
  virtual size_t GetMaxInputSize() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
