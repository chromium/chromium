// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_ENCODER_FALLBACK_H_
#define MEDIA_VIDEO_VIDEO_ENCODER_FALLBACK_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "media/base/media_export.h"
#include "media/base/video_encoder.h"

namespace media {

// A proxy video encoder that delegates all work to the main encoder until it
// fails. If the main encoder fails during initialization or encoding,
// VideoEncoderFallback creates a fallback encoder (by calling
// create_fallback_cb()) and starts delegating all the work to it.
//
// It is used for switching from accelerated platform encoder to softare encoder
// which are more reliable but less efficient.
class MEDIA_EXPORT VideoEncoderFallback : public VideoEncoder {
 public:
  using CreateFallbackCB = base::OnceCallback<std::unique_ptr<VideoEncoder>()>;

  VideoEncoderFallback(std::unique_ptr<VideoEncoder> main_encoder,
                       CreateFallbackCB create_fallback_cb);
  ~VideoEncoderFallback() override;

  // VideoEncoder implementation.
  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              bool key_frame,
              EncoderStatusCB done_cb) override;
  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     EncoderStatusCB done_cb) override;
  void Flush(EncoderStatusCB done_cb) override;

 private:
  void FallbackInitialize();
  void FallbackEncode(PendingEncode args);
  void FallbackInitCompleted(EncoderStatus status);
  PendingEncode MakePendingEncode(scoped_refptr<VideoFrame> frame,
                                  bool key_frame,
                                  EncoderStatusCB done_cb);
  void CallOutput(VideoEncoderOutput output,
                  absl::optional<CodecDescription> desc);

  std::unique_ptr<VideoEncoder> encoder_;

  // True when |main_encoder| provided to ctor() failed and we initiated
  // transition to a fallback encoder (created by |create_fallback_cb_|).
  bool use_fallback_ = false;

  // True when the fallback encoder was successfully initialized.
  bool fallback_initialized_ = false;

  // Pending encodes that need to be retried once the fallback encoder is
  // initialized.
  std::vector<std::unique_ptr<PendingEncode>> encodes_to_retry_;

  CreateFallbackCB create_fallback_cb_;
  EncoderStatusCB init_done_cb_;
  OutputCB output_cb_;
  VideoCodecProfile profile_;
  Options options_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<VideoEncoderFallback> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_ENCODER_FALLBACK_H_
