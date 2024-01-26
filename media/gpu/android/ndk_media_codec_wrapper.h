// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_NDK_MEDIA_CODEC_WRAPPER_H_
#define MEDIA_GPU_ANDROID_NDK_MEDIA_CODEC_WRAPPER_H_

#include <media/NdkMediaCodec.h>

#include <memory>
#include <string_view>

#include "base/android/requires_api.h"
#include "base/containers/circular_deque.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "media/gpu/media_gpu_export.h"

// For use with REQUIRES_ANDROID_API() and __builtin_available().
// We need at least Android P for AMediaCodec_getInputFormat(), but in
// Android P we have issues with CFI and dynamic linker on arm64. However
// GetSupportedProfiles() needs Q+, so just limit to Q.
#define NDK_MEDIA_CODEC_MIN_API 29

namespace media {

struct AMediaCodecDeleter {
  inline void operator()(AMediaCodec* ptr) const {
    if (ptr) {
      AMediaCodec_delete(ptr);
    }
  }
};

// A wrapper class which manages async callbacks from an AMediaCodec, as
// well as queues of available input/output buffers.
class REQUIRES_ANDROID_API(NDK_MEDIA_CODEC_MIN_API)
    MEDIA_GPU_EXPORT NdkMediaCodecWrapper {
 public:
  class Client {
   public:
    // Called when a new input buffer is made available.
    // Retrieve the buffer using Inputs_TakeFront().
    virtual void OnInputAvailable() = 0;

    // Called when a new output buffer is made available.
    // Retrieve the buffer using Outputs_TakeFront() or Outputs_PeekFront().
    virtual void OnOutputAvailable() = 0;

    // Called when there was an asynchronous encoding error.
    virtual void OnError(media_status_t error) = 0;
  };

  using BufferIndex = int32_t;

  // Info about output buffers currently pending in media codec.
  struct OutputInfo {
    BufferIndex buffer_index;
    AMediaCodecBufferInfo info;
  };

  // Creates a MediaCodecWrapper, returning nullptr on failure.
  // `client` must outlive the returned MediaCodecWrapper.
  // The `runner` sequence is used to invoke all of `client`'s async methods.
  static std::unique_ptr<NdkMediaCodecWrapper> CreateByCodecName(
      std::string_view codec_name,
      Client* client,
      scoped_refptr<base::SequencedTaskRunner> runner);
  static std::unique_ptr<NdkMediaCodecWrapper> CreateByMimeType(
      std::string_view mime_type,
      Client* client,
      scoped_refptr<base::SequencedTaskRunner> runner);

  NdkMediaCodecWrapper(const NdkMediaCodecWrapper&) = delete;
  NdkMediaCodecWrapper& operator=(const NdkMediaCodecWrapper&) = delete;

  ~NdkMediaCodecWrapper();

  // Starts or stops the underlying `media_codec_` and its async callbacks.
  // The async callbacks will post to `client_` on `task_runner_`.
  // Note: Before calling Start(), `media_codec_` should have already been
  //      configured using AMediaCodec_configure() and the codec() accessor.
  // Note: Stop() must be called before calling Start() again.
  media_status_t Start();
  void Stop();

  // Returns whether there are any available input/output buffers.
  bool HasInput();
  bool HasOutput();

  // Returns the first available input/output buffer and removes it from the
  // queue of available buffers.
  [[nodiscard]] BufferIndex TakeInput();
  [[nodiscard]] OutputInfo TakeOutput();

  // Returns the first available output buffer without removing it form the
  // available buffer queue.
  OutputInfo PeekOutput();

  // Returns the underlying codec directly for use with AMediaCodec_* functions.
  // Note: Do not call AMediaCodec_{start|stop}() directly. Instead, use the
  //      provided Start()/Stop() methods, which also handle setting up and
  //      tearing down the underlying async callbacks.
  AMediaCodec* codec() { return media_codec_.get(); }

 private:
  friend class NdkMediaCodecWrapperTest;

  using MediaCodecPtr = std::unique_ptr<AMediaCodec, AMediaCodecDeleter>;

  NdkMediaCodecWrapper(MediaCodecPtr codec,
                       Client* client,
                       scoped_refptr<base::SequencedTaskRunner> runner);

  // Called by MediaCodec when an input buffer becomes available.
  static void OnAsyncInputAvailable(AMediaCodec* codec,
                                    void* userdata,
                                    int32_t index);
  void OnInputAvailable(int32_t index);

  // Called by MediaCodec when an output buffer becomes available.
  static void OnAsyncOutputAvailable(AMediaCodec* codec,
                                     void* userdata,
                                     int32_t index,
                                     AMediaCodecBufferInfo* bufferInfo);
  void OnOutputAvailable(int32_t index, AMediaCodecBufferInfo bufferInfo);

  // Called by MediaCodec when the output format has changed.
  static void OnAsyncFormatChanged(AMediaCodec* codec,
                                   void* userdata,
                                   AMediaFormat* format) {}

  // Called when the MediaCodec encountered an error.
  static void OnAsyncError(AMediaCodec* codec,
                           void* userdata,
                           media_status_t error,
                           int32_t actionCode,
                           const char* detail);
  void OnError(media_status_t error);

  SEQUENCE_CHECKER(sequence_checker_);

  // Only used for error checking the start/stop state.
  bool started_ = false;

  // A runner all for callbacks and externals calls to public methods.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  MediaCodecPtr media_codec_;

  // Outlives `this`, as it owns `this`.
  const raw_ptr<Client> client_;

  // Indices of input buffers currently pending in media codec.
  base::circular_deque<BufferIndex> input_buffers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::circular_deque<OutputInfo> output_buffers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtr<NdkMediaCodecWrapper> weak_this_;
  base::WeakPtrFactory<NdkMediaCodecWrapper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_NDK_MEDIA_CODEC_WRAPPER_H_
