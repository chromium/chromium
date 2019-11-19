// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "media/base/encryption_pattern.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_export.h"
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
enum MediaCodecStatus {
  MEDIA_CODEC_OK,
  MEDIA_CODEC_TRY_AGAIN_LATER,
  MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED,
  MEDIA_CODEC_OUTPUT_FORMAT_CHANGED,
  MEDIA_CODEC_NO_KEY,
  MEDIA_CODEC_ERROR
};

// An interface for a bridge to an Android MediaCodec.
class MEDIA_EXPORT MediaCodecBridge {
 public:
  MediaCodecBridge() = default;
  virtual ~MediaCodecBridge() = default;

  // Calls MediaCodec#stop(). However, due to buggy implementations (b/8125974)
  // Stop() followed by Start() may not work on some devices. For reliability,
  // it's recommended to delete the instance and create a new one instead.
  virtual void Stop() = 0;

  // Calls MediaCodec#flush(). The codec takes ownership of all input and output
  // buffers previously dequeued when this is called. Returns MEDIA_CODEC_ERROR
  // if an unexpected error happens, or MEDIA_CODEC_OK otherwise.
  virtual MediaCodecStatus Flush() = 0;

  // Returns the output size. This is valid after DequeueOutputBuffer()
  // signals a format change by returning OUTPUT_FORMAT_CHANGED.
  // Returns MEDIA_CODEC_ERROR if an error occurs, or MEDIA_CODEC_OK otherwise.
  virtual MediaCodecStatus GetOutputSize(gfx::Size* size) = 0;

  // Gets the sampling rate. This is valid after DequeueOutputBuffer()
  // signals a format change by returning INFO_OUTPUT_FORMAT_CHANGED.
  // Returns MEDIA_CODEC_ERROR if an error occurs, or MEDIA_CODEC_OK otherwise.
  virtual MediaCodecStatus GetOutputSamplingRate(int* sampling_rate) = 0;

  // Fills |channel_count| with the number of audio channels.  This is valid
  // after DequeueOutputBuffer() signals a format change by returning
  // INFO_OUTPUT_FORMAT_CHANGED. Returns MEDIA_CODEC_ERROR if an error occurs,
  // or MEDIA_CODEC_OK otherwise.
  virtual MediaCodecStatus GetOutputChannelCount(int* channel_count) = 0;

  // Submits a byte array to the given input buffer. Call this after getting an
  // available buffer from DequeueInputBuffer(). If |data| is NULL, it assumes
  // the input buffer has already been populated (but still obeys |size|).
  // |data_size| must be less than kint32max (because Java).
  virtual MediaCodecStatus QueueInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      base::TimeDelta presentation_time) = 0;

  // As above but for encrypted buffers. NULL |subsamples| indicates the
  // whole buffer is encrypted.
  virtual MediaCodecStatus QueueSecureInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const std::string& key_id,
      const std::string& iv,
      const std::vector<SubsampleEntry>& subsamples,
      EncryptionScheme encryption_scheme,
      base::Optional<EncryptionPattern> encryption_pattern,
      base::TimeDelta presentation_time) = 0;

  // Submits an empty buffer with the END_OF_STREAM flag set.
  virtual void QueueEOS(int input_buffer_index) = 0;

  // Returns:
  // MEDIA_CODEC_OK if an input buffer is ready to be filled with valid data,
  // MEDIA_CODEC_ENQUEUE_INPUT_AGAIN_LATER if no such buffer is available, or
  // MEDIA_CODEC_ERROR if unexpected error happens.
  virtual MediaCodecStatus DequeueInputBuffer(base::TimeDelta timeout,
                                              int* index) = 0;

  // Dequeues an output buffer, block for up to |timeout|.
  // Returns the status of this operation. If OK is returned, the output
  // parameters should be populated. Otherwise, the values of output parameters
  // should not be used.  Output parameters other than index/offset/size are
  // optional and only set if not NULL.
  virtual MediaCodecStatus DequeueOutputBuffer(
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
  virtual MediaCodecStatus GetInputBuffer(int input_buffer_index,
                                          uint8_t** data,
                                          size_t* capacity) = 0;

  // Copies |num| bytes from output buffer |index|'s |offset| into the memory
  // region pointed to by |dst|. To avoid overflows, the size of both source
  // and destination must be at least |num| bytes, and should not overlap.
  // Returns MEDIA_CODEC_ERROR if an error occurs, or MEDIA_CODEC_OK otherwise.
  virtual MediaCodecStatus CopyFromOutputBuffer(int index,
                                                size_t offset,
                                                void* dst,
                                                size_t num) = 0;

  // Gets the component name. Before API level 18 this returns an empty string.
  virtual std::string GetName() = 0;

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

  DISALLOW_COPY_AND_ASSIGN(MediaCodecBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
