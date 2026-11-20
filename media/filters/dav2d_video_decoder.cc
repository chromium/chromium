// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/dav2d_video_decoder.h"

#include <errno.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_color_space.h"
#include "media/base/video_types.h"
#include "ui/gfx/color_space.h"

extern "C" {
#include "third_party/dav2d/libdav2d/include/dav2d/dav2d.h"
}

namespace media {

namespace {

// std::unique_ptr release helpers. We need to release both the containing
// structs as well as refs held within the structures.
struct ScopedDav2dDataFree {
  void operator()(void* x) const {
    auto* data = static_cast<Dav2dData*>(x);
    dav2d_data_unref(data);
    delete data;
  }
};

struct ScopedDav2dPictureFree {
  void operator()(void* x) const {
    auto* pic = static_cast<Dav2dPicture*>(x);
    dav2d_picture_unref(pic);
    delete pic;
  }
};

}  // namespace

// static
int Dav2dVideoDecoder::AllocPicture(Dav2dPicture* p, void* frame_pool_opaque) {
  VideoChromaSampling chroma_sampling = VideoChromaSampling::kUnknown;
  switch (p->p.layout) {
    case DAV2D_PIXEL_LAYOUT_I400:
      chroma_sampling = VideoChromaSampling::k400;
      break;
    case DAV2D_PIXEL_LAYOUT_I420:
      chroma_sampling = VideoChromaSampling::k420;
      break;
    case DAV2D_PIXEL_LAYOUT_I422:
      chroma_sampling = VideoChromaSampling::k422;
      break;
    case DAV2D_PIXEL_LAYOUT_I444:
      chroma_sampling = VideoChromaSampling::k444;
      break;
  }

  const int res = AllocateCustomPicture(p, DAV2D_PICTURE_ALIGNMENT,
                                        chroma_sampling, frame_pool_opaque);
  return res < 0 ? DAV2D_ERR(ENOMEM) : 0;
}

Dav2dVideoDecoder::Dav2dVideoDecoder(std::unique_ptr<MediaLog> media_log,
                                     OffloadState offload_state)
    : DavXdVideoDecoderBase(std::move(media_log), offload_state) {}

Dav2dVideoDecoder::~Dav2dVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseDecoder();
}

VideoDecoderType Dav2dVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kDav2d;
}

VideoCodec Dav2dVideoDecoder::GetSupportedCodec() const {
  return VideoCodec::kAV2;
}

DecoderStatus Dav2dVideoDecoder::OpenDecoder(bool low_delay, int n_threads) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Dav2dSettings s;
  dav2d_default_settings(&s);

  // Setup a custom allocator so OOM failures error out instead of crashing.
  s.allocator.cookie = frame_pool();
  s.allocator.alloc_picture_callback = &AllocPicture;
  s.allocator.release_picture_callback = &ReleaseCustomPicture<Dav2dPicture>;

  s.n_threads = n_threads;

  // We only want 1 frame thread in low delay mode, since otherwise we'll
  // require at least two buffers before the first frame can be output.
  if (low_delay) {
    s.max_frame_delay = 1;
  }

  // Only output the highest spatial layer.
  s.all_layers = 0;

  // Route dav2d internal logs through Chrome's DLOG system.
  s.logger = {nullptr, &LogDecoderMessage};

  // Set a maximum frame size limit to avoid OOM'ing fuzzers.
  s.frame_size_limit = limits::kMaxCanvas;

  {
    Dav2dContext* decoder = nullptr;
    const int res = dav2d_open(&decoder, &s);
    if (res < 0) {
      return DecoderStatus(DecoderStatus::Codes::kFailedToCreateDecoder,
                           "dav2d_open() failed")
          .WithData("error_code", res);
    }
    dav2d_decoder_.reset(decoder);
  }
  extra_data_sent_ = false;
  return DecoderStatus::Codes::kOk;
}

void Dav2dVideoDecoder::CloseCodec() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dav2d_decoder_.reset();
  extra_data_sent_ = false;
  pts_queue_ = {};
}

void Dav2dVideoDecoder::FlushCodec() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dav2d_flush(dav2d_decoder_.get());
  extra_data_sent_ = false;
  pts_queue_ = {};
}

void Dav2dVideoDecoder::Dav2dContextDeleter::operator()(Dav2dContext* ptr) {
  dav2d_close(&ptr);
}

bool Dav2dVideoDecoder::DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  using ScopedPtrDav2dData = std::unique_ptr<Dav2dData, ScopedDav2dDataFree>;

  if (!extra_data_sent_) {
    extra_data_sent_ = true;
    if (!config().extra_data().empty()) {
      auto extra_data_buffer = DecoderBuffer::CopyFrom(config().extra_data());
      auto extra_data_span = base::span(*extra_data_buffer);
      ScopedPtrDav2dData extra_data(new Dav2dData{});
      const int res = dav2d_data_wrap(
          extra_data.get(), extra_data_span.data(), extra_data_span.size(),
          &ReleaseDecoderBuffer, extra_data_buffer.get());
      if (res < 0) {
        if (res == DAV2D_ERR(ENOMEM)) {
          SetErrorStatus(DecoderStatus::Codes::kOutOfMemory);
        }
        MEDIA_LOG(ERROR, media_log())
            << "dav2d_data_wrap() failed on extra_data with error " << res;
        return false;
      }
      extra_data_buffer->AddRef();

      if (extra_data->sz) {
        const int send_res =
            dav2d_send_data(dav2d_decoder_.get(), extra_data.get());
        if (send_res < 0 && send_res != DAV2D_ERR(EAGAIN)) {
          MEDIA_LOG(ERROR, media_log())
              << "dav2d_send_data() failed on extra_data with error "
              << send_res;
          return false;
        }
      }
    }
  }

  ScopedPtrDav2dData input_buffer;
  if (!buffer->end_of_stream()) {
    RecordBufferMetadata(*buffer);

    auto buffer_span = base::span(*buffer);
    input_buffer.reset(new Dav2dData{});
    const int res = dav2d_data_wrap(input_buffer.get(), buffer_span.data(),
                                    buffer_span.size(), &ReleaseDecoderBuffer,
                                    buffer.get());
    if (res < 0) {
      if (res == DAV2D_ERR(ENOMEM)) {
        SetErrorStatus(DecoderStatus::Codes::kOutOfMemory);
      }
      MEDIA_LOG(ERROR, media_log())
          << "dav2d_data_wrap() failed with error " << res;
      return false;
    }
    input_buffer->m.timestamp = buffer->timestamp().InMicroseconds();
    if (buffer->timestamp() != kNoTimestamp) {
      pts_queue_.push(buffer->timestamp());
    }
    buffer->AddRef();
  }

  // Used to DCHECK that dav2d_send_data() actually takes the packet. If we exit
  // this function without sending |input_buffer| that packet will be lost. We
  // have no packet to send at end of stream.
  bool send_data_completed = buffer->end_of_stream();

  if (buffer->end_of_stream()) {
    dav2d_send_data(dav2d_decoder_.get(), nullptr);
  }

  while (!input_buffer || input_buffer->sz) {
    if (input_buffer) {
      const int res = dav2d_send_data(dav2d_decoder_.get(), input_buffer.get());
      if (res < 0 && res != DAV2D_ERR(EAGAIN)) {
        MEDIA_LOG(ERROR, media_log())
            << "dav2d_send_data() failed with error " << res << " on "
            << buffer->AsHumanReadableString();
        return false;
      }

      if (res != DAV2D_ERR(EAGAIN)) {
        send_data_completed = true;
      }

      // Even if dav2d_send_data() returned EAGAIN, try dav2d_get_picture().
    }

    using ScopedPtrDav2dPicture =
        std::unique_ptr<Dav2dPicture, ScopedDav2dPictureFree>;
    ScopedPtrDav2dPicture p(new Dav2dPicture{});

    const int res = dav2d_get_picture(dav2d_decoder_.get(), p.get());
    if (res < 0) {
      if (res != DAV2D_ERR(EAGAIN) && res != DAV2D_EOF) {
        MEDIA_LOG(ERROR, media_log())
            << "dav2d_get_picture() failed with error " << res << " on "
            << buffer->AsHumanReadableString();
        return false;
      }

      // We've reached end of stream and no frames remain to drain.
      if (!input_buffer) {
        DCHECK(send_data_completed);
        return true;
      }

      continue;
    }

    if (p->m.timestamp == INT64_MIN) {
      if (pts_queue_.empty()) {
        MEDIA_LOG(ERROR, media_log())
            << "dav2d_get_picture() produced a frame with no timestamp.";
        return false;
      }
      p->m.timestamp = pts_queue_.top().InMicroseconds();
      pts_queue_.pop();
    } else {
      const base::TimeDelta pic_timestamp = base::Microseconds(p->m.timestamp);
      while (!pts_queue_.empty() && pts_queue_.top() <= pic_timestamp) {
        pts_queue_.pop();
      }
    }

    VideoColorSpace color_space;
    if (p->ci && p->ci->color_description_present) {
      color_space = VideoColorSpace(
          p->ci->color.pri, p->ci->color.trc, p->ci->color.mtrx,
          p->ci->color.range ? gfx::ColorSpace::RangeID::FULL
                             : gfx::ColorSpace::RangeID::LIMITED);
    }

    if (!OutputDecodedPicture(*p, color_space)) {
      return false;
    }
  }

  DCHECK(send_data_completed);
  return true;
}

}  // namespace media
