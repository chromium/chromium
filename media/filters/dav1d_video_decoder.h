// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DAV1D_VIDEO_DECODER_H_
#define MEDIA_FILTERS_DAV1D_VIDEO_DECODER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "media/base/decoder_status.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/filters/davxd_video_decoder_base.h"
#include "media/filters/offloading_video_decoder.h"

struct Dav1dContext;
struct Dav1dPicture;

namespace media {

class DecoderBuffer;

class MEDIA_EXPORT Dav1dVideoDecoder : public DavXdVideoDecoderBase {
 public:
  explicit Dav1dVideoDecoder(
      std::unique_ptr<MediaLog> media_log,
      OffloadState offload_state = OffloadState::kNormal);

  Dav1dVideoDecoder(const Dav1dVideoDecoder&) = delete;
  Dav1dVideoDecoder& operator=(const Dav1dVideoDecoder&) = delete;

  ~Dav1dVideoDecoder() override;

  // VideoDecoder implementation.
  VideoDecoderType GetDecoderType() const override;

 protected:
  // DavXdVideoDecoderBase implementation.
  VideoCodec GetSupportedCodec() const override;
  DecoderStatus OpenDecoder(bool low_delay, int n_threads) override;
  void CloseCodec() override;
  void FlushCodec() override;
  bool DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) override;

 private:
  static int AllocPicture(Dav1dPicture* p, void* frame_pool_opaque);

  // The allocated decoder; null before Initialize() and anytime after
  // CloseCodec().
  struct Dav1dContextDeleter {
    void operator()(Dav1dContext* ptr);
  };
  std::unique_ptr<Dav1dContext, Dav1dContextDeleter> dav1d_decoder_;
};

// Helper class for creating a Dav1dVideoDecoder which will offload all AV1
// content from the media thread.
class OffloadingDav1dVideoDecoder : public OffloadingVideoDecoder {
 public:
  explicit OffloadingDav1dVideoDecoder(std::unique_ptr<MediaLog> media_log)
      : OffloadingVideoDecoder(
            0,
            std::vector<VideoCodec>(1, VideoCodec::kAV1),
            std::make_unique<Dav1dVideoDecoder>(
                std::move(media_log),
                OffloadableVideoDecoder::OffloadState::kOffloaded)) {}
};

}  // namespace media

#endif  // MEDIA_FILTERS_DAV1D_VIDEO_DECODER_H_
