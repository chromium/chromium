// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ASYNC_DESTROY_VIDEO_DECODER_H_
#define MEDIA_BASE_ASYNC_DESTROY_VIDEO_DECODER_H_

#include <memory>
#include <type_traits>
#include "media/base/video_decoder.h"

namespace media {

// Some VideoDecoder implementations must do non-synchronous cleanup before
// they are destroyed. This wrapper implementation allows a VideoDecoder
// to schedule its own cleanup tasks before its memory is released.
// The underlying type must implement a static
// `DestroyAsync(std::unique_ptr<T>)` function which fires any pending
// callbacks, stops and destroys the decoder. After this call, external
// resources (e.g. raw pointers) held by the decoder might be invalidated
// immediately. So if the decoder is destroyed asynchronously (e.g. DeleteSoon),
// external resources must be released in this call.
template <typename T>
class AsyncDestroyVideoDecoder final : public VideoDecoder {
 public:
  explicit AsyncDestroyVideoDecoder(std::unique_ptr<T> wrapped_decoder)
      : wrapped_decoder_(std::move(wrapped_decoder)) {
    static_assert(std::is_base_of<VideoDecoder, T>::value,
                  "T must implement 'media::VideoDecoder'");
    DCHECK(wrapped_decoder_);
  }

  ~AsyncDestroyVideoDecoder() override {
    if (wrapped_decoder_)
      T::DestroyAsync(std::move(wrapped_decoder_));
  }

  VideoDecoderType GetDecoderType() const override {
    DCHECK(wrapped_decoder_);
    return wrapped_decoder_->GetDecoderType();
  }

  bool IsPlatformDecoder() const override {
    DCHECK(wrapped_decoder_);
    return wrapped_decoder_->IsPlatformDecoder();
  }

  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override {
    DCHECK(wrapped_decoder_);
    wrapped_decoder_->Initialize(config, low_delay, cdm_context,
                                 std::move(init_cb), output_cb, waiting_cb);
  }

  void Decode(scoped_refptr<DecoderBuffer> buffer,
              DecodeCB decode_cb) override {
    DCHECK(wrapped_decoder_);
    wrapped_decoder_->Decode(std::move(buffer), std::move(decode_cb));
  }

  void Reset(base::OnceClosure closure) override {
    DCHECK(wrapped_decoder_);
    wrapped_decoder_->Reset(std::move(closure));
  }

  bool NeedsBitstreamConversion() const override {
    DCHECK(wrapped_decoder_);
    return wrapped_decoder_->NeedsBitstreamConversion();
  }

  bool CanReadWithoutStalling() const override {
    DCHECK(wrapped_decoder_);
    return wrapped_decoder_->CanReadWithoutStalling();
  }

  int GetMaxDecodeRequests() const override {
    DCHECK(wrapped_decoder_);
    return wrapped_decoder_->GetMaxDecodeRequests();
  }

  bool FramesHoldExternalResources() const override {
    DCHECK(wrapped_decoder_);
    return wrapped_decoder_->FramesHoldExternalResources();
  }

 private:
  std::unique_ptr<T> wrapped_decoder_;
};

}  // namespace media

#endif  // MEDIA_BASE_ASYNC_DESTROY_VIDEO_DECODER_H_
