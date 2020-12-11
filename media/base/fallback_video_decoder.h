// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FALLBACK_VIDEO_DECODER_H_
#define MEDIA_BASE_FALLBACK_VIDEO_DECODER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "media/base/video_decoder.h"

namespace media {

// A Wrapper VideoDecoder which supports a fallback and a preferred decoder.
class MEDIA_EXPORT FallbackVideoDecoder : public VideoDecoder {
 public:
  FallbackVideoDecoder(std::unique_ptr<VideoDecoder> preferred,
                       std::unique_ptr<VideoDecoder> fallback);

  // media::VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

 protected:
  ~FallbackVideoDecoder() override;

 private:
  void FallbackInitialize(const VideoDecoderConfig& config,
                          bool low_delay,
                          CdmContext* cdm_context,
                          InitCB init_cb,
                          const OutputCB& output_cb,
                          const WaitingCB& waiting_cb,
                          Status status);

  std::unique_ptr<media::VideoDecoder> preferred_decoder_;
  std::unique_ptr<media::VideoDecoder> fallback_decoder_;
  media::VideoDecoder* selected_decoder_ = nullptr;
  bool did_fallback_ = false;

  base::WeakPtrFactory<FallbackVideoDecoder> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FallbackVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_BASE_FALLBACK_VIDEO_DECODER_H_
