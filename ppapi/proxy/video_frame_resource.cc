// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/video_frame_resource.h"

#include "base/logging.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

VideoFrameResource::VideoFrameResource(PP_Instance instance,
                                       int32_t index,
                                       MediaStreamBuffer* buffer)
    : Resource(OBJECT_IS_PROXY, instance),
      index_(index),
      buffer_(buffer) {
  DCHECK_EQ(buffer_->header.type, MediaStreamBuffer::TYPE_VIDEO);
}

VideoFrameResource::~VideoFrameResource() {
  CHECK(!buffer_) << "An unused (or unrecycled) frame is destroyed.";
}

thunk::PPB_VideoFrame_API* VideoFrameResource::AsPPB_VideoFrame_API() {
  return this;
}

PP_TimeDelta VideoFrameResource::GetTimestamp() {
  if (!buffer_) {
    VLOG(1) << "Frame is invalid";
    return 0.0;
  }
  return buffer_->video.timestamp;
}

void VideoFrameResource::SetTimestamp(PP_TimeDelta timestamp) {
  if (!buffer_) {
    VLOG(1) << "Frame is invalid";
    return;
  }
  buffer_->video.timestamp = timestamp;
}

PP_VideoFrame_Format VideoFrameResource::GetFormat() {
  if (!buffer_) {
    VLOG(1) << "Frame is invalid";
    return PP_VIDEOFRAME_FORMAT_UNKNOWN;
  }
  return buffer_->video.format;
}

PP_Bool VideoFrameResource::GetSize(PP_Size* size) {
  if (!buffer_) {
    VLOG(1) << "Frame is invalid";
    return PP_FALSE;
  }
  *size = buffer_->video.size;
  return PP_TRUE;
}

void* VideoFrameResource::GetDataBuffer() {
  if (!buffer_) {
    VLOG(1) << "Frame is invalid";
    return NULL;
  }
  return buffer_->video.data;
}

uint32_t VideoFrameResource::GetDataBufferSize() {
  if (!buffer_) {
    VLOG(1) << "Frame is invalid";
    return 0;
  }
  return buffer_->video.data_size;
}

MediaStreamBuffer* VideoFrameResource::GetBuffer() {
  return buffer_;
}

int32_t VideoFrameResource::GetBufferIndex() {
  return index_;
}

void VideoFrameResource::Invalidate() {
  DCHECK(buffer_);
  DCHECK_GE(index_, 0);
  buffer_ = NULL;
  index_ = -1;
}

}  // namespace proxy
}  // namespace ppapi
