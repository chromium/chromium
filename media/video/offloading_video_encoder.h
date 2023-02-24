// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_OFFLOADING_VIDEO_ENCODER_H_
#define MEDIA_VIDEO_OFFLOADING_VIDEO_ENCODER_H_

#include <memory>
#include <type_traits>

#include "base/sequence_checker.h"
#include "media/base/video_encoder.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

// A wrapper around video encoder that offloads all the calls to a dedicated
// task runner. It's used to move synchronous software encoding work off the
// current (main) thread.
class MEDIA_EXPORT OffloadingVideoEncoder final : public VideoEncoder {
 public:
  // |work_runner| - task runner for encoding work
  // |callback_runner| - all encoder's callbacks will be executed on this task
  // runner.
  OffloadingVideoEncoder(
      std::unique_ptr<VideoEncoder> wrapped_encoder,
      const scoped_refptr<base::SequencedTaskRunner> work_runner,
      const scoped_refptr<base::SequencedTaskRunner> callback_runner);

  // Uses current task runner for callbacks and asks thread pool for a new task
  // runner to do actual encoding work.
  explicit OffloadingVideoEncoder(
      std::unique_ptr<VideoEncoder> wrapped_encoder);

  ~OffloadingVideoEncoder() override;

  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  EncoderInfoCB info_cb,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override;

  void Encode(scoped_refptr<VideoFrame> frame,
              const EncodeOptions& encode_options,
              EncoderStatusCB done_cb) override;

  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     EncoderStatusCB done_cb) override;

  void Flush(EncoderStatusCB done_cb) override;

 private:
  template <class T>
  T WrapCallback(T cb);

  std::unique_ptr<VideoEncoder> wrapped_encoder_;
  const scoped_refptr<base::SequencedTaskRunner> work_runner_;
  const scoped_refptr<base::SequencedTaskRunner> callback_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_VIDEO_OFFLOADING_VIDEO_ENCODER_H_
