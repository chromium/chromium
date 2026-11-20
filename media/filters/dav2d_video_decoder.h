// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DAV2D_VIDEO_DECODER_H_
#define MEDIA_FILTERS_DAV2D_VIDEO_DECODER_H_

#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/decoder_status.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/filters/davxd_video_decoder_base.h"
#include "media/filters/offloading_video_decoder.h"

struct Dav2dContext;
struct Dav2dPicture;

namespace media {

class DecoderBuffer;

class MEDIA_EXPORT Dav2dVideoDecoder : public DavXdVideoDecoderBase {
 public:
  explicit Dav2dVideoDecoder(
      std::unique_ptr<MediaLog> media_log,
      OffloadState offload_state = OffloadState::kNormal);

  Dav2dVideoDecoder(const Dav2dVideoDecoder&) = delete;
  Dav2dVideoDecoder& operator=(const Dav2dVideoDecoder&) = delete;

  ~Dav2dVideoDecoder() override;

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
  static int AllocPicture(Dav2dPicture* p, void* frame_pool_opaque);

  // The allocated decoder; null before Initialize() and anytime after
  // CloseCodec().
  struct Dav2dContextDeleter {
    void operator()(Dav2dContext* ptr);
  };
  std::unique_ptr<Dav2dContext, Dav2dContextDeleter> dav2d_decoder_;

  bool extra_data_sent_ = false;

  // Workaround for libdav2d failing to propagate pic.m.timestamp on output.
  std::priority_queue<base::TimeDelta,
                      std::vector<base::TimeDelta>,
                      std::greater<base::TimeDelta>>
      pts_queue_;
};

// Helper class for creating a Dav2dVideoDecoder which will offload all AV2
// content from the media thread.
class OffloadingDav2dVideoDecoder : public OffloadingVideoDecoder {
 public:
  explicit OffloadingDav2dVideoDecoder(std::unique_ptr<MediaLog> media_log)
      : OffloadingVideoDecoder(
            0,
            std::vector<VideoCodec>(1, VideoCodec::kAV2),
            std::make_unique<Dav2dVideoDecoder>(
                std::move(media_log),
                OffloadableVideoDecoder::OffloadState::kOffloaded)) {}
};

}  // namespace media

#endif  // MEDIA_FILTERS_DAV2D_VIDEO_DECODER_H_
