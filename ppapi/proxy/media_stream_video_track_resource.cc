// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/media_stream_video_track_resource.h"

#include "base/bind.h"
#include "base/logging.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/video_frame_resource.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/shared_impl/media_stream_video_track_shared.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

MediaStreamVideoTrackResource::MediaStreamVideoTrackResource(
    Connection connection,
    PP_Instance instance,
    int pending_renderer_id,
    const std::string& id)
    : MediaStreamTrackResourceBase(
        connection, instance, pending_renderer_id, id),
      get_frame_output_(NULL) {
}

MediaStreamVideoTrackResource::MediaStreamVideoTrackResource(
    Connection connection,
    PP_Instance instance)
    : MediaStreamTrackResourceBase(connection, instance),
      get_frame_output_(NULL) {
  SendCreate(RENDERER, PpapiHostMsg_MediaStreamVideoTrack_Create());
}

MediaStreamVideoTrackResource::~MediaStreamVideoTrackResource() {
  Close();
}

thunk::PPB_MediaStreamVideoTrack_API*
MediaStreamVideoTrackResource::AsPPB_MediaStreamVideoTrack_API() {
  return this;
}

PP_Var MediaStreamVideoTrackResource::GetId() {
  return StringVar::StringToPPVar(id());
}

PP_Bool MediaStreamVideoTrackResource::HasEnded() {
  return PP_FromBool(has_ended());
}

int32_t MediaStreamVideoTrackResource::Configure(
    const int32_t attrib_list[],
    scoped_refptr<TrackedCallback> callback) {
  if (has_ended())
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(configure_callback_) ||
      TrackedCallback::IsPending(get_frame_callback_)) {
    return PP_ERROR_INPROGRESS;
  }

  // Do not support configure, if frames are hold by plugin.
  if (!frames_.empty())
    return PP_ERROR_INPROGRESS;

  MediaStreamVideoTrackShared::Attributes attributes;
  int i = 0;
  for (;attrib_list[i] != PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE; i += 2) {
    switch (attrib_list[i]) {
    case PP_MEDIASTREAMVIDEOTRACK_ATTRIB_BUFFERED_FRAMES:
      attributes.buffers = attrib_list[i + 1];
      break;
    case PP_MEDIASTREAMVIDEOTRACK_ATTRIB_WIDTH:
      attributes.width = attrib_list[i + 1];
      break;
    case PP_MEDIASTREAMVIDEOTRACK_ATTRIB_HEIGHT:
      attributes.height = attrib_list[i + 1];
      break;
    case PP_MEDIASTREAMVIDEOTRACK_ATTRIB_FORMAT:
      attributes.format = static_cast<PP_VideoFrame_Format>(attrib_list[i + 1]);
      break;
    default:
      return PP_ERROR_BADARGUMENT;
    }
  }

  if (!MediaStreamVideoTrackShared::VerifyAttributes(attributes))
    return PP_ERROR_BADARGUMENT;

  configure_callback_ = callback;
  Call<PpapiPluginMsg_MediaStreamVideoTrack_ConfigureReply>(
      RENDERER,
      PpapiHostMsg_MediaStreamVideoTrack_Configure(attributes),
      base::Bind(&MediaStreamVideoTrackResource::OnPluginMsgConfigureReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t MediaStreamVideoTrackResource::GetAttrib(
    PP_MediaStreamVideoTrack_Attrib attrib,
    int32_t* value) {
  // TODO(penghuang): implement this function.
  return PP_ERROR_NOTSUPPORTED;
}

int32_t MediaStreamVideoTrackResource::GetFrame(
    PP_Resource* frame,
    scoped_refptr<TrackedCallback> callback) {
  if (has_ended())
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(configure_callback_) ||
      TrackedCallback::IsPending(get_frame_callback_)) {
    return PP_ERROR_INPROGRESS;
  }

  *frame = GetVideoFrame();
  if (*frame)
    return PP_OK;

  get_frame_output_ = frame;
  get_frame_callback_ = callback;
  return PP_OK_COMPLETIONPENDING;
}

int32_t MediaStreamVideoTrackResource::RecycleFrame(PP_Resource frame) {
  FrameMap::iterator it = frames_.find(frame);
  if (it == frames_.end())
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<VideoFrameResource> frame_resource = it->second;
  frames_.erase(it);

  if (has_ended())
    return PP_OK;

  DCHECK_GE(frame_resource->GetBufferIndex(), 0);

  SendEnqueueBufferMessageToHost(frame_resource->GetBufferIndex());
  frame_resource->Invalidate();
  return PP_OK;
}

void MediaStreamVideoTrackResource::Close() {
  if (has_ended())
    return;

  if (TrackedCallback::IsPending(get_frame_callback_)) {
    *get_frame_output_ = 0;
    get_frame_callback_->PostAbort();
    get_frame_callback_.reset();
    get_frame_output_ = 0;
  }

  ReleaseFrames();
  MediaStreamTrackResourceBase::CloseInternal();
}

int32_t MediaStreamVideoTrackResource::GetEmptyFrame(
    PP_Resource* frame, scoped_refptr<TrackedCallback> callback) {
  return GetFrame(frame, callback);
}

int32_t MediaStreamVideoTrackResource::PutFrame(PP_Resource frame) {
  // TODO(ronghuawu): Consider to rename RecycleFrame to PutFrame and use
  // one set of GetFrame and PutFrame for both input and output.
  return RecycleFrame(frame);
}

void MediaStreamVideoTrackResource::OnNewBufferEnqueued() {
  if (!TrackedCallback::IsPending(get_frame_callback_))
    return;

  *get_frame_output_ = GetVideoFrame();
  int32_t result = *get_frame_output_ ? PP_OK : PP_ERROR_FAILED;
  get_frame_output_ = NULL;
  scoped_refptr<TrackedCallback> callback;
  callback.swap(get_frame_callback_);
  callback->Run(result);
}

PP_Resource MediaStreamVideoTrackResource::GetVideoFrame() {
  int32_t index = buffer_manager()->DequeueBuffer();
  if (index < 0)
    return 0;

  MediaStreamBuffer* buffer = buffer_manager()->GetBufferPointer(index);
  DCHECK(buffer);
  scoped_refptr<VideoFrameResource> resource =
      new VideoFrameResource(pp_instance(), index, buffer);
  // Add |pp_resource()| and |resource| into |frames_|.
  // |frames_| uses std::unique_ptr<> to hold a ref of |resource|. It keeps the
  // resource alive.
  frames_.insert(FrameMap::value_type(resource->pp_resource(), resource));
  return resource->GetReference();
}

void MediaStreamVideoTrackResource::ReleaseFrames() {
  for (FrameMap::iterator it = frames_.begin(); it != frames_.end(); ++it) {
    // Just invalidate and release VideoFrameResorce, but keep PP_Resource.
    // So plugin can still use |RecycleFrame()|.
    it->second->Invalidate();
    it->second.reset();
  }
}

void MediaStreamVideoTrackResource::OnPluginMsgConfigureReply(
    const ResourceMessageReplyParams& params,
    const std::string& track_id) {
  if (id().empty()) {
    set_id(track_id);
  } else {
    DCHECK_EQ(id(), track_id);
  }
  if (TrackedCallback::IsPending(configure_callback_)) {
    scoped_refptr<TrackedCallback> callback;
    callback.swap(configure_callback_);
    callback->Run(params.result());
  }
}

}  // namespace proxy
}  // namespace ppapi
