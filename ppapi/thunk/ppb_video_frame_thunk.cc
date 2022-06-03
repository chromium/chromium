// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_video_frame.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_video_frame_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsVideoFrame(PP_Resource resource) {
  VLOG(4) << "PPB_VideoFrame::IsVideoFrame()";
  EnterResource<PPB_VideoFrame_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_TimeDelta GetTimestamp(PP_Resource frame) {
  VLOG(4) << "PPB_VideoFrame::GetTimestamp()";
  EnterResource<PPB_VideoFrame_API> enter(frame, true);
  if (enter.failed())
    return 0.0;
  return enter.object()->GetTimestamp();
}

void SetTimestamp(PP_Resource frame, PP_TimeDelta timestamp) {
  VLOG(4) << "PPB_VideoFrame::SetTimestamp()";
  EnterResource<PPB_VideoFrame_API> enter(frame, true);
  if (enter.failed())
    return;
  enter.object()->SetTimestamp(timestamp);
}

PP_VideoFrame_Format GetFormat(PP_Resource frame) {
  VLOG(4) << "PPB_VideoFrame::GetFormat()";
  EnterResource<PPB_VideoFrame_API> enter(frame, true);
  if (enter.failed())
    return PP_VIDEOFRAME_FORMAT_UNKNOWN;
  return enter.object()->GetFormat();
}

PP_Bool GetSize(PP_Resource frame, struct PP_Size* size) {
  VLOG(4) << "PPB_VideoFrame::GetSize()";
  EnterResource<PPB_VideoFrame_API> enter(frame, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetSize(size);
}

void* GetDataBuffer(PP_Resource frame) {
  VLOG(4) << "PPB_VideoFrame::GetDataBuffer()";
  EnterResource<PPB_VideoFrame_API> enter(frame, true);
  if (enter.failed())
    return NULL;
  return enter.object()->GetDataBuffer();
}

uint32_t GetDataBufferSize(PP_Resource frame) {
  VLOG(4) << "PPB_VideoFrame::GetDataBufferSize()";
  EnterResource<PPB_VideoFrame_API> enter(frame, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetDataBufferSize();
}

const PPB_VideoFrame_0_1 g_ppb_videoframe_thunk_0_1 = {
    &IsVideoFrame, &GetTimestamp,  &SetTimestamp,     &GetFormat,
    &GetSize,      &GetDataBuffer, &GetDataBufferSize};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_VideoFrame_0_1* GetPPB_VideoFrame_0_1_Thunk() {
  return &g_ppb_videoframe_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
