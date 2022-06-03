// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_media_stream_video_track.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_media_stream_video_track_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateMediaStreamVideoTrack(instance);
}

PP_Bool IsMediaStreamVideoTrack(PP_Resource resource) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::IsMediaStreamVideoTrack()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Configure(PP_Resource video_track,
                  const int32_t attrib_list[],
                  struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::Configure()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(video_track, callback,
                                                     true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->Configure(attrib_list, enter.callback()));
}

int32_t GetAttrib(PP_Resource video_track,
                  PP_MediaStreamVideoTrack_Attrib attrib,
                  int32_t* value) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::GetAttrib()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(video_track, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetAttrib(attrib, value);
}

struct PP_Var GetId(PP_Resource video_track) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::GetId()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(video_track, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetId();
}

PP_Bool HasEnded(PP_Resource video_track) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::HasEnded()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(video_track, true);
  if (enter.failed())
    return PP_TRUE;
  return enter.object()->HasEnded();
}

int32_t GetFrame(PP_Resource video_track,
                 PP_Resource* frame,
                 struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::GetFrame()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(video_track, callback,
                                                     true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetFrame(frame, enter.callback()));
}

int32_t RecycleFrame(PP_Resource video_track, PP_Resource frame) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::RecycleFrame()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(video_track, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->RecycleFrame(frame);
}

void Close(PP_Resource video_track) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::Close()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(video_track, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

int32_t GetEmptyFrame(PP_Resource video_track,
                      PP_Resource* frame,
                      struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::GetEmptyFrame()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(video_track, callback,
                                                     true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetEmptyFrame(frame, enter.callback()));
}

int32_t PutFrame(PP_Resource video_track, PP_Resource frame) {
  VLOG(4) << "PPB_MediaStreamVideoTrack::PutFrame()";
  EnterResource<PPB_MediaStreamVideoTrack_API> enter(video_track, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->PutFrame(frame);
}

const PPB_MediaStreamVideoTrack_0_1 g_ppb_mediastreamvideotrack_thunk_0_1 = {
    &IsMediaStreamVideoTrack,
    &Configure,
    &GetAttrib,
    &GetId,
    &HasEnded,
    &GetFrame,
    &RecycleFrame,
    &Close};

const PPB_MediaStreamVideoTrack_1_0 g_ppb_mediastreamvideotrack_thunk_1_0 = {
    &Create,    &IsMediaStreamVideoTrack,
    &Configure, &GetAttrib,
    &GetId,     &HasEnded,
    &GetFrame,  &RecycleFrame,
    &Close,     &GetEmptyFrame,
    &PutFrame};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_MediaStreamVideoTrack_0_1*
GetPPB_MediaStreamVideoTrack_0_1_Thunk() {
  return &g_ppb_mediastreamvideotrack_thunk_0_1;
}

PPAPI_THUNK_EXPORT const PPB_MediaStreamVideoTrack_1_0*
GetPPB_MediaStreamVideoTrack_1_0_Thunk() {
  return &g_ppb_mediastreamvideotrack_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
