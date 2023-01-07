// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/media_stream_video_track.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/video_frame.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_MediaStreamVideoTrack_1_0>() {
  return PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_MediaStreamVideoTrack_0_1>() {
  return PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_0_1;
}

}  // namespace

MediaStreamVideoTrack::MediaStreamVideoTrack() {
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    const MediaStreamVideoTrack& other) : Resource(other) {
}

MediaStreamVideoTrack::MediaStreamVideoTrack(const Resource& resource)
    : Resource(resource) {
  PP_DCHECK(IsMediaStreamVideoTrack(resource));
}

MediaStreamVideoTrack::MediaStreamVideoTrack(const InstanceHandle& instance) {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    PassRefFromConstructor(
        get_interface<PPB_MediaStreamVideoTrack_1_0>()->Create(
            instance.pp_instance()));
    return;
  }
  PP_DCHECK(false);
}

MediaStreamVideoTrack::MediaStreamVideoTrack(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
}

int32_t MediaStreamVideoTrack::Configure(
    const int32_t attributes[],
    const CompletionCallback& callback) {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    return get_interface<PPB_MediaStreamVideoTrack_1_0>()->Configure(
        pp_resource(), attributes, callback.pp_completion_callback());
  } else if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return get_interface<PPB_MediaStreamVideoTrack_0_1>()->Configure(
        pp_resource(), attributes, callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t MediaStreamVideoTrack::GetAttrib(PP_MediaStreamVideoTrack_Attrib attrib,
                                         int32_t* value) {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    return get_interface<PPB_MediaStreamVideoTrack_1_0>()->GetAttrib(
        pp_resource(), attrib, value);
  } else if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return get_interface<PPB_MediaStreamVideoTrack_0_1>()->GetAttrib(
        pp_resource(), attrib, value);
  }
  return PP_ERROR_NOINTERFACE;
}

std::string MediaStreamVideoTrack::GetId() const {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    pp::Var id(PASS_REF, get_interface<PPB_MediaStreamVideoTrack_1_0>()->GetId(
        pp_resource()));
    if (id.is_string())
      return id.AsString();
  } else if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    pp::Var id(PASS_REF, get_interface<PPB_MediaStreamVideoTrack_0_1>()->GetId(
        pp_resource()));
    if (id.is_string())
      return id.AsString();
  }
  return std::string();
}

bool MediaStreamVideoTrack::HasEnded() const {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    return PP_ToBool(get_interface<PPB_MediaStreamVideoTrack_1_0>()->HasEnded(
        pp_resource()));
  } else if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return PP_ToBool(get_interface<PPB_MediaStreamVideoTrack_0_1>()->HasEnded(
        pp_resource()));
  }
  return true;
}

int32_t MediaStreamVideoTrack::GetFrame(
    const CompletionCallbackWithOutput<VideoFrame>& callback) {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    return get_interface<PPB_MediaStreamVideoTrack_1_0>()->GetFrame(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  } else if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return get_interface<PPB_MediaStreamVideoTrack_0_1>()->GetFrame(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t MediaStreamVideoTrack::RecycleFrame(const VideoFrame& frame) {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    return get_interface<PPB_MediaStreamVideoTrack_1_0>()->RecycleFrame(
        pp_resource(), frame.pp_resource());
  } else if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return get_interface<PPB_MediaStreamVideoTrack_0_1>()->RecycleFrame(
        pp_resource(), frame.pp_resource());
  }
  return PP_ERROR_NOINTERFACE;
}

void MediaStreamVideoTrack::Close() {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>())
    get_interface<PPB_MediaStreamVideoTrack_1_0>()->Close(pp_resource());
  else if (has_interface<PPB_MediaStreamVideoTrack_0_1>())
    get_interface<PPB_MediaStreamVideoTrack_0_1>()->Close(pp_resource());

}

int32_t MediaStreamVideoTrack::GetEmptyFrame(
    const CompletionCallbackWithOutput<VideoFrame>& callback) {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    return get_interface<PPB_MediaStreamVideoTrack_1_0>()->GetEmptyFrame(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t MediaStreamVideoTrack::PutFrame(const VideoFrame& frame) {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    return get_interface<PPB_MediaStreamVideoTrack_1_0>()->PutFrame(
        pp_resource(), frame.pp_resource());
  }
  return PP_ERROR_NOINTERFACE;
}

bool MediaStreamVideoTrack::IsMediaStreamVideoTrack(const Resource& resource) {
  if (has_interface<PPB_MediaStreamVideoTrack_1_0>()) {
    return PP_ToBool(get_interface<PPB_MediaStreamVideoTrack_1_0>()->
        IsMediaStreamVideoTrack(resource.pp_resource()));
  } else if (has_interface<PPB_MediaStreamVideoTrack_0_1>()) {
    return PP_ToBool(get_interface<PPB_MediaStreamVideoTrack_0_1>()->
        IsMediaStreamVideoTrack(resource.pp_resource()));
  }
  return false;
}

}  // namespace pp
