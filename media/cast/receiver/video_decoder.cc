// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/video_decoder.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_util.h"
#include "media/cast/cast_environment.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace cast {

// Base class that handles the common problem of detecting dropped frames, and
// then invoking the Decode() method implemented by the subclasses to convert
// the encoded payload data into a usable video frame.
class VideoDecoder::ImplBase
    : public base::RefCountedThreadSafe<VideoDecoder::ImplBase> {
 public:
  ImplBase(const scoped_refptr<CastEnvironment>& cast_environment, Codec codec)
      : cast_environment_(cast_environment),
        codec_(codec),
        operational_status_(STATUS_UNINITIALIZED) {}

  OperationalStatus InitializationResult() const {
    return operational_status_;
  }

  void DecodeFrame(std::unique_ptr<EncodedFrame> encoded_frame,
                   const DecodeFrameCallback& callback) {
    DCHECK_EQ(operational_status_, STATUS_INITIALIZED);

    bool is_continuous = true;
    DCHECK(!encoded_frame->frame_id.is_null());
    if (!last_frame_id_.is_null()) {
      if (encoded_frame->frame_id > (last_frame_id_ + 1)) {
        RecoverBecauseFramesWereDropped();
        is_continuous = false;
      }
    }
    last_frame_id_ = encoded_frame->frame_id;

    const scoped_refptr<VideoFrame> decoded_frame = Decode(
        encoded_frame->mutable_bytes(),
        static_cast<int>(encoded_frame->data.size()));
    if (!decoded_frame) {
      VLOG(2) << "Decoding of frame " << encoded_frame->frame_id << " failed.";
      cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
                                  base::Bind(callback, decoded_frame, false));
      return;
    }
    decoded_frame->set_timestamp(
        encoded_frame->rtp_timestamp.ToTimeDelta(kVideoFrequency));

    std::unique_ptr<FrameEvent> decode_event(new FrameEvent());
    decode_event->timestamp = cast_environment_->Clock()->NowTicks();
    decode_event->type = FRAME_DECODED;
    decode_event->media_type = VIDEO_EVENT;
    decode_event->rtp_timestamp = encoded_frame->rtp_timestamp;
    decode_event->frame_id = encoded_frame->frame_id;
    cast_environment_->logger()->DispatchFrameEvent(std::move(decode_event));

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(callback, decoded_frame, is_continuous));
  }

 protected:
  friend class base::RefCountedThreadSafe<ImplBase>;
  virtual ~ImplBase() = default;

  virtual void RecoverBecauseFramesWereDropped() {}

  // Note: Implementation of Decode() is allowed to mutate |data|.
  virtual scoped_refptr<VideoFrame> Decode(uint8_t* data, int len) = 0;

  const scoped_refptr<CastEnvironment> cast_environment_;
  const Codec codec_;

  // Subclass' ctor is expected to set this to STATUS_INITIALIZED.
  OperationalStatus operational_status_;

  // Pool of VideoFrames to decode incoming frames into.
  media::VideoFramePool video_frame_pool_;

 private:
  FrameId last_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(ImplBase);
};

class VideoDecoder::Vp8Impl : public VideoDecoder::ImplBase {
 public:
  explicit Vp8Impl(const scoped_refptr<CastEnvironment>& cast_environment)
      : ImplBase(cast_environment, CODEC_VIDEO_VP8) {
    if (ImplBase::operational_status_ != STATUS_UNINITIALIZED)
      return;

    vpx_codec_dec_cfg_t cfg = {0};
    // TODO(miu): Revisit this for typical multi-core desktop use case.  This
    // feels like it should be 4 or 8.
    cfg.threads = 1;

    DCHECK(vpx_codec_get_caps(vpx_codec_vp8_dx()) & VPX_CODEC_CAP_POSTPROC);
    if (vpx_codec_dec_init(&context_,
                           vpx_codec_vp8_dx(),
                           &cfg,
                           VPX_CODEC_USE_POSTPROC) != VPX_CODEC_OK) {
      ImplBase::operational_status_ = STATUS_INVALID_CONFIGURATION;
      return;
    }
    ImplBase::operational_status_ = STATUS_INITIALIZED;
  }

 private:
  ~Vp8Impl() final {
    if (ImplBase::operational_status_ == STATUS_INITIALIZED)
      CHECK_EQ(VPX_CODEC_OK, vpx_codec_destroy(&context_));
  }

  scoped_refptr<VideoFrame> Decode(uint8_t* data, int len) final {
    if (len <= 0 ||
        vpx_codec_decode(&context_, data, static_cast<unsigned int>(len),
                         nullptr, 0) != VPX_CODEC_OK) {
      return nullptr;
    }

    vpx_codec_iter_t iter = nullptr;
    vpx_image_t* const image = vpx_codec_get_frame(&context_, &iter);
    if (!image)
      return nullptr;
    if (image->fmt != VPX_IMG_FMT_I420) {
      NOTREACHED() << "Only pixel format supported is I420, got " << image->fmt;
      return nullptr;
    }
    DCHECK(vpx_codec_get_frame(&context_, &iter) == nullptr)
        << "Should have only decoded exactly one frame.";

    const gfx::Size frame_size(image->d_w, image->d_h);
    // Note: Timestamp for the VideoFrame will be set in VideoReceiver.
    // |decoded_frame| will be returned to |video_frame_pool_| on destruction to
    // be reused.
    const scoped_refptr<VideoFrame> decoded_frame =
        video_frame_pool_.CreateFrame(PIXEL_FORMAT_I420, frame_size,
                                      gfx::Rect(frame_size), frame_size,
                                      base::TimeDelta());
    libyuv::I420Copy(image->planes[VPX_PLANE_Y], image->stride[VPX_PLANE_Y],
                     image->planes[VPX_PLANE_U], image->stride[VPX_PLANE_U],
                     image->planes[VPX_PLANE_V], image->stride[VPX_PLANE_V],
                     decoded_frame->visible_data(media::VideoFrame::kYPlane),
                     decoded_frame->stride(media::VideoFrame::kYPlane),
                     decoded_frame->visible_data(media::VideoFrame::kUPlane),
                     decoded_frame->stride(media::VideoFrame::kUPlane),
                     decoded_frame->visible_data(media::VideoFrame::kVPlane),
                     decoded_frame->stride(media::VideoFrame::kVPlane),
                     frame_size.width(), frame_size.height());
    return decoded_frame;
  }

  // VPX decoder context (i.e., an instantiation).
  vpx_codec_ctx_t context_;

  DISALLOW_COPY_AND_ASSIGN(Vp8Impl);
};

#ifndef OFFICIAL_BUILD
// A fake video decoder that always output 2x2 black frames.
class VideoDecoder::FakeImpl : public VideoDecoder::ImplBase {
 public:
  explicit FakeImpl(const scoped_refptr<CastEnvironment>& cast_environment)
      : ImplBase(cast_environment, CODEC_VIDEO_FAKE),
        last_decoded_id_(-1) {
    if (ImplBase::operational_status_ != STATUS_UNINITIALIZED)
      return;
    ImplBase::operational_status_ = STATUS_INITIALIZED;
  }

 private:
  ~FakeImpl() final = default;

  scoped_refptr<VideoFrame> Decode(uint8_t* data, int len) final {
    // Make sure this is a JSON string.
    if (!len || data[0] != '{')
      return nullptr;
    std::unique_ptr<base::Value> values(base::JSONReader::ReadDeprecated(
        base::StringPiece(reinterpret_cast<char*>(data), len)));
    if (!values)
      return nullptr;
    base::DictionaryValue* dict = nullptr;
    values->GetAsDictionary(&dict);

    bool key = false;
    int id = 0;
    int ref = 0;
    dict->GetBoolean("key", &key);
    dict->GetInteger("id", &id);
    dict->GetInteger("ref", &ref);
    DCHECK(id == last_decoded_id_ + 1);
    last_decoded_id_ = id;
    return media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2));
  }

  int last_decoded_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeImpl);
};
#endif

VideoDecoder::VideoDecoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    Codec codec)
    : cast_environment_(cast_environment) {
  switch (codec) {
#ifndef OFFICIAL_BUILD
    case CODEC_VIDEO_FAKE:
      impl_ = new FakeImpl(cast_environment);
      break;
#endif
    case CODEC_VIDEO_VP8:
      impl_ = new Vp8Impl(cast_environment);
      break;
    case CODEC_VIDEO_H264:
      // TODO(miu): Need implementation.
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED() << "Unknown or unspecified codec.";
      break;
  }
}

VideoDecoder::~VideoDecoder() = default;

OperationalStatus VideoDecoder::InitializationResult() const {
  if (impl_.get())
    return impl_->InitializationResult();
  return STATUS_UNSUPPORTED_CODEC;
}

void VideoDecoder::DecodeFrame(std::unique_ptr<EncodedFrame> encoded_frame,
                               const DecodeFrameCallback& callback) {
  DCHECK(encoded_frame.get());
  DCHECK(!callback.is_null());
  if (!impl_.get() || impl_->InitializationResult() != STATUS_INITIALIZED) {
    callback.Run(base::WrapRefCounted<VideoFrame>(nullptr), false);
    return;
  }
  cast_environment_->PostTask(CastEnvironment::VIDEO,
                              FROM_HERE,
                              base::Bind(&VideoDecoder::ImplBase::DecodeFrame,
                                         impl_,
                                         base::Passed(&encoded_frame),
                                         callback));
}

}  // namespace cast
}  // namespace media
