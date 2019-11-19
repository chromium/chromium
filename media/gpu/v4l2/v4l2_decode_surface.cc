// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_decode_surface.h"

#include <linux/media.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "media/gpu/macros.h"

namespace media {

V4L2DecodeSurface::V4L2DecodeSurface(V4L2WritableBufferRef input_buffer,
                                     V4L2WritableBufferRef output_buffer,
                                     scoped_refptr<VideoFrame> frame)
    : input_record_(input_buffer.BufferId()),
      output_record_(output_buffer.BufferId()),
      decoded_(false) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  input_buffer_ = std::move(input_buffer);
  output_buffer_ = std::move(output_buffer);
  video_frame_ = std::move(frame);
}

V4L2DecodeSurface::~V4L2DecodeSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOGF(5) << "Releasing output record id=" << output_record_;
  if (release_cb_)
    std::move(release_cb_).Run();
}

void V4L2DecodeSurface::SetDecoded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!decoded_);
  decoded_ = true;

  // We can now drop references to all reference surfaces for this surface
  // as we are done with decoding.
  reference_surfaces_.clear();

  // And finally execute and drop the decode done callback, if set.
  if (done_cb_)
    std::move(done_cb_).Run();
}

void V4L2DecodeSurface::SetVisibleRect(const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  visible_rect_ = visible_rect;
}

void V4L2DecodeSurface::SetReferenceSurfaces(
    std::vector<scoped_refptr<V4L2DecodeSurface>> ref_surfaces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(reference_surfaces_.empty());
#if DCHECK_IS_ON()
  for (const auto& ref : reference_surfaces_)
    DCHECK_NE(ref->output_record(), output_record_);
#endif

  reference_surfaces_ = std::move(ref_surfaces);
}

void V4L2DecodeSurface::SetDecodeDoneCallback(base::OnceClosure done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!done_cb_);

  done_cb_ = std::move(done_cb);
}

void V4L2DecodeSurface::SetReleaseCallback(base::OnceClosure release_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  release_cb_ = std::move(release_cb);
}

std::string V4L2DecodeSurface::ToString() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string out;
  base::StringAppendF(&out, "Buffer %d -> %d. ", input_record_, output_record_);
  base::StringAppendF(&out, "Reference surfaces:");
  for (const auto& ref : reference_surfaces_) {
    DCHECK_NE(ref->output_record(), output_record_);
    base::StringAppendF(&out, " %d", ref->output_record());
  }
  return out;
}

void V4L2ConfigStoreDecodeSurface::PrepareSetCtrls(
    struct v4l2_ext_controls* ctrls) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(ctrls, nullptr);
  DCHECK_GT(config_store_, 0u);

  ctrls->config_store = config_store_;
}

void V4L2ConfigStoreDecodeSurface::PrepareQueueBuffer(
    struct v4l2_buffer* buffer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(buffer, nullptr);
  DCHECK_GT(config_store_, 0u);

  buffer->config_store = config_store_;
}

uint64_t V4L2ConfigStoreDecodeSurface::GetReferenceID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Control store uses the output buffer ID as reference.
  return output_record();
}

bool V4L2ConfigStoreDecodeSurface::Submit() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // There is nothing extra to submit when using the config store
  return true;
}

// static
base::Optional<scoped_refptr<V4L2RequestDecodeSurface>>
V4L2RequestDecodeSurface::Create(V4L2WritableBufferRef input_buffer,
                                 V4L2WritableBufferRef output_buffer,
                                 scoped_refptr<VideoFrame> frame,
                                 int request_fd) {
  constexpr int kPollTimeoutMs = 500;
  int ret;
  struct pollfd poll_fd = {request_fd, POLLPRI, 0};

  // First poll the request to ensure its previous task is done
  ret = poll(&poll_fd, 1, kPollTimeoutMs);
  if (ret != 1) {
    VPLOGF(1) << "Failed to poll request: ";
    return base::nullopt;
  }

  // Then reinit the request to make sure we can use it for a new submission.
  ret = HANDLE_EINTR(ioctl(request_fd, MEDIA_REQUEST_IOC_REINIT));
  if (ret < 0) {
    VPLOGF(1) << "Failed to reinit request: ";
    return base::nullopt;
  }

  return new V4L2RequestDecodeSurface(std::move(input_buffer),
                                      std::move(output_buffer),
                                      std::move(frame), request_fd);
}

void V4L2RequestDecodeSurface::PrepareSetCtrls(
    struct v4l2_ext_controls* ctrls) const {
  DCHECK_NE(ctrls, nullptr);
  DCHECK_GE(request_fd_, 0);

  ctrls->which = V4L2_CTRL_WHICH_REQUEST_VAL;
  ctrls->request_fd = request_fd_;
}

void V4L2RequestDecodeSurface::PrepareQueueBuffer(
    struct v4l2_buffer* buffer) const {
  DCHECK_NE(buffer, nullptr);
  DCHECK_GE(request_fd_, 0);

  buffer->request_fd = request_fd_;
  buffer->flags |= V4L2_BUF_FLAG_REQUEST_FD;
  // Use the output buffer index as the timestamp.
  // Since the client is supposed to keep the output buffer out of the V4L2
  // queue for as long as it is used as a reference frame, this ensures that
  // all the requests we submit have unique IDs at any point in time.
  DCHECK_EQ(static_cast<int>(buffer->index), input_record());
  buffer->timestamp.tv_sec = 0;
  buffer->timestamp.tv_usec = output_record();
}

uint64_t V4L2RequestDecodeSurface::GetReferenceID() const {
  // Convert the input buffer ID to what the internal representation of
  // the timestamp we submitted will be (tv_usec * 1000).
  return output_record() * 1000;
}

bool V4L2RequestDecodeSurface::Submit() const {
  DCHECK_GE(request_fd_, 0);

  int ret = HANDLE_EINTR(ioctl(request_fd_, MEDIA_REQUEST_IOC_QUEUE));
  return ret == 0;
}

}  // namespace media
