// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/video_frame.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VideoFrame_0_1>() {
  return PPB_VIDEOFRAME_INTERFACE_0_1;
}

}

VideoFrame::VideoFrame() {
}

VideoFrame::VideoFrame(const VideoFrame& other) : Resource(other) {
}

VideoFrame::VideoFrame(const Resource& resource) : Resource(resource) {
}

VideoFrame::VideoFrame(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

VideoFrame::~VideoFrame() {
}

PP_TimeDelta VideoFrame::GetTimestamp() const {
  if (has_interface<PPB_VideoFrame_0_1>())
    return get_interface<PPB_VideoFrame_0_1>()->GetTimestamp(pp_resource());
  return 0.0;
}

void VideoFrame::SetTimestamp(PP_TimeDelta timestamp) {
  if (has_interface<PPB_VideoFrame_0_1>())
    get_interface<PPB_VideoFrame_0_1>()->SetTimestamp(pp_resource(), timestamp);
}

PP_VideoFrame_Format VideoFrame::GetFormat() const {
  if (has_interface<PPB_VideoFrame_0_1>())
    return get_interface<PPB_VideoFrame_0_1>()->GetFormat(pp_resource());
  return PP_VIDEOFRAME_FORMAT_UNKNOWN;
}

bool VideoFrame::GetSize(Size* size) const {
  if (has_interface<PPB_VideoFrame_0_1>())
    return PP_ToBool(get_interface<PPB_VideoFrame_0_1>()->GetSize(
        pp_resource(), &size->pp_size()));
  return false;
}

void* VideoFrame::GetDataBuffer() {
  if (has_interface<PPB_VideoFrame_0_1>())
    return get_interface<PPB_VideoFrame_0_1>()->GetDataBuffer(pp_resource());
  return NULL;
}

uint32_t VideoFrame::GetDataBufferSize() const {
  if (has_interface<PPB_VideoFrame_0_1>()) {
    return get_interface<PPB_VideoFrame_0_1>()->GetDataBufferSize(
        pp_resource());
  }
  return 0;
}

}  // namespace pp
