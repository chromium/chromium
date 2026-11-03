// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/dav1d_video_decoder.h"

#include <errno.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/sequence_checker.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/video_color_space.h"
#include "media/base/video_types.h"
#include "ui/gfx/color_space.h"

extern "C" {
#include "third_party/dav1d/libdav1d/include/dav1d/dav1d.h"
}

namespace media {

namespace {

// std::unique_ptr release helpers. We need to release both the containing
// structs as well as refs held within the structures.
struct ScopedDav1dDataFree {
  void operator()(void* x) const {
    auto* data = static_cast<Dav1dData*>(x);
    dav1d_data_unref(data);
    delete data;
  }
};

struct ScopedDav1dPictureFree {
  void operator()(void* x) const {
    auto* pic = static_cast<Dav1dPicture*>(x);
    dav1d_picture_unref(pic);
    delete pic;
  }
};

}  // namespace

// static
int Dav1dVideoDecoder::AllocPicture(Dav1dPicture* p, void* frame_pool_opaque) {
  VideoChromaSampling chroma_sampling = VideoChromaSampling::kUnknown;
  switch (p->p.layout) {
    case DAV1D_PIXEL_LAYOUT_I400:
      chroma_sampling = VideoChromaSampling::k400;
      break;
    case DAV1D_PIXEL_LAYOUT_I420:
      chroma_sampling = VideoChromaSampling::k420;
      break;
    case DAV1D_PIXEL_LAYOUT_I422:
      chroma_sampling = VideoChromaSampling::k422;
      break;
    case DAV1D_PIXEL_LAYOUT_I444:
      chroma_sampling = VideoChromaSampling::k444;
      break;
  }

  const int res = AllocateCustomPicture(p, DAV1D_PICTURE_ALIGNMENT,
                                        chroma_sampling, frame_pool_opaque);
  return res < 0 ? DAV1D_ERR(ENOMEM) : 0;
}

Dav1dVideoDecoder::Dav1dVideoDecoder(std::unique_ptr<MediaLog> media_log,
                                     OffloadState offload_state)
    : DavXdVideoDecoderBase(std::move(media_log), offload_state) {}

Dav1dVideoDecoder::~Dav1dVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseDecoder();
}

VideoDecoderType Dav1dVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kDav1d;
}

VideoCodec Dav1dVideoDecoder::GetSupportedCodec() const {
  return VideoCodec::kAV1;
}

DecoderStatus Dav1dVideoDecoder::OpenDecoder(bool low_delay, int n_threads) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Dav1dSettings s;
  dav1d_default_settings(&s);

  // Setup a custom allocator so OOM failures error out instead of crashing.
  s.allocator.cookie = frame_pool();
  s.allocator.alloc_picture_callback = &AllocPicture;
  s.allocator.release_picture_callback = &ReleaseCustomPicture<Dav1dPicture>;

  s.n_threads = n_threads;

  // We only want 1 frame thread in low delay mode, since otherwise we'll
  // require at least two buffers before the first frame can be output.
  if (low_delay) {
    s.max_frame_delay = 1;
  }

  // Only output the highest spatial layer.
  s.all_layers = 0;

  // Route dav1d internal logs through Chrome's DLOG system.
  s.logger = {nullptr, &LogDecoderMessage};

  // Set a maximum frame size limit to avoid OOM'ing fuzzers.
  s.frame_size_limit = limits::kMaxCanvas;

  {
    Dav1dContext* decoder = nullptr;
    const int res = dav1d_open(&decoder, &s);
    if (res < 0) {
      return DecoderStatus(DecoderStatus::Codes::kFailedToCreateDecoder,
                           "dav1d_open() failed")
          .WithData("error_code", res);
    }
    dav1d_decoder_.reset(decoder);
  }
  return DecoderStatus::Codes::kOk;
}

void Dav1dVideoDecoder::CloseCodec() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dav1d_decoder_.reset();
}

void Dav1dVideoDecoder::FlushCodec() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dav1d_flush(dav1d_decoder_.get());
}

void Dav1dVideoDecoder::Dav1dContextDeleter::operator()(Dav1dContext* ptr) {
  dav1d_close(&ptr);
}

bool Dav1dVideoDecoder::DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  using ScopedPtrDav1dData = std::unique_ptr<Dav1dData, ScopedDav1dDataFree>;
  ScopedPtrDav1dData input_buffer;
  if (!buffer->end_of_stream()) {
    RecordBufferMetadata(*buffer);

    auto buffer_span = base::span(*buffer);
    input_buffer.reset(new Dav1dData{});
    const int res = dav1d_data_wrap(input_buffer.get(), buffer_span.data(),
                                    buffer_span.size(), &ReleaseDecoderBuffer,
                                    buffer.get());
    if (res < 0) {
      if (res == DAV1D_ERR(ENOMEM)) {
        SetErrorStatus(DecoderStatus::Codes::kOutOfMemory);
      }
      MEDIA_LOG(ERROR, media_log())
          << "dav1d_data_wrap() failed with error " << res;
      return false;
    }
    input_buffer->m.timestamp = buffer->timestamp().InMicroseconds();
    buffer->AddRef();
  }

  // Used to DCHECK that dav1d_send_data() actually takes the packet. If we exit
  // this function without sending |input_buffer| that packet will be lost. We
  // have no packet to send at end of stream.
  bool send_data_completed = buffer->end_of_stream();

  while (!input_buffer || input_buffer->sz) {
    if (input_buffer) {
      const int res = dav1d_send_data(dav1d_decoder_.get(), input_buffer.get());
      if (res < 0 && res != -EAGAIN) {
        MEDIA_LOG(ERROR, media_log())
            << "dav1d_send_data() failed with error " << res << " on "
            << buffer->AsHumanReadableString();
        return false;
      }

      if (res != -EAGAIN) {
        send_data_completed = true;
      }

      // Even if dav1d_send_data() returned EAGAIN, try dav1d_get_picture().
    }

    using ScopedPtrDav1dPicture =
        std::unique_ptr<Dav1dPicture, ScopedDav1dPictureFree>;
    ScopedPtrDav1dPicture p(new Dav1dPicture{});

    const int res = dav1d_get_picture(dav1d_decoder_.get(), p.get());
    if (res < 0) {
      if (res != -EAGAIN) {
        MEDIA_LOG(ERROR, media_log())
            << "dav1d_get_picture() failed with error " << res << " on "
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

    // AV1 color space defines match ISO 23001-8:2016 via ISO/IEC 23091-4/ITU-T
    // H.273. https://aomediacodec.github.io/av1-spec/#color-config-semantics
    VideoColorSpace color_space;
    if (p->seq_hdr) {
      color_space = VideoColorSpace(
          p->seq_hdr->pri, p->seq_hdr->trc, p->seq_hdr->mtrx,
          p->seq_hdr->color_range ? gfx::ColorSpace::RangeID::FULL
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
