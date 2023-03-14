// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ASYNC_DESTROY_VIDEO_ENCODER_H_
#define MEDIA_BASE_ASYNC_DESTROY_VIDEO_ENCODER_H_

#include <memory>
#include <type_traits>

#include "media/base/video_encoder.h"

namespace media {

// Some VideoEncoder implementations must do non-synchronous cleanup before
// they are destroyed. This wrapper implementation allows a VideoEncoder
// to schedule its own cleanup tasks before its memory is released.
// The underlying type must implement a static
// `DestroyAsync(std::unique_ptr<T>)` function which fires any pending
// callbacks, stops and destroys the encoder. After this call, external
// resources (e.g. raw pointers) held by the encoder might be invalidated
// immediately. So if the encoder is destroyed asynchronously (e.g. DeleteSoon),
// external resources must be released in this call.
template <typename T>
class AsyncDestroyVideoEncoder final : public VideoEncoder {
 public:
  explicit AsyncDestroyVideoEncoder(std::unique_ptr<T> wrapped_encoder)
      : wrapped_encoder_(std::move(wrapped_encoder)) {
    static_assert(std::is_base_of<VideoEncoder, T>::value,
                  "T must implement 'media::VideoEncoder'");
    DCHECK(wrapped_encoder_);
  }

  ~AsyncDestroyVideoEncoder() override {
    if (wrapped_encoder_)
      T::DestroyAsync(std::move(wrapped_encoder_));
  }

  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  EncoderInfoCB info_cb,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override {
    DCHECK(wrapped_encoder_);
    wrapped_encoder_->Initialize(profile, options, std::move(info_cb),
                                 std::move(output_cb), std::move(done_cb));
  }

  void Encode(scoped_refptr<VideoFrame> frame,
              const EncodeOptions& options,
              EncoderStatusCB done_cb) override {
    DCHECK(wrapped_encoder_);
    wrapped_encoder_->Encode(std::move(frame), options, std::move(done_cb));
  }

  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     EncoderStatusCB done_cb) override {
    DCHECK(wrapped_encoder_);
    wrapped_encoder_->ChangeOptions(options, std::move(output_cb),
                                    std::move(done_cb));
  }

  void Flush(EncoderStatusCB done_cb) override {
    DCHECK(wrapped_encoder_);
    wrapped_encoder_->Flush(std::move(done_cb));
  }

 private:
  std::unique_ptr<T> wrapped_encoder_;
};

}  // namespace media

#endif  // MEDIA_BASE_ASYNC_DESTROY_VIDEO_ENCODER_H_
