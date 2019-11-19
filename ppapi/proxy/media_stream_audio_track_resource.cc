// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/media_stream_audio_track_resource.h"

#include "base/bind.h"
#include "ppapi/proxy/audio_buffer_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_audio_track_shared.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

MediaStreamAudioTrackResource::MediaStreamAudioTrackResource(
    Connection connection,
    PP_Instance instance,
    int pending_renderer_id,
    const std::string& id)
    : MediaStreamTrackResourceBase(
        connection, instance, pending_renderer_id, id),
      get_buffer_output_(NULL) {
}

MediaStreamAudioTrackResource::~MediaStreamAudioTrackResource() {
  Close();
}

thunk::PPB_MediaStreamAudioTrack_API*
MediaStreamAudioTrackResource::AsPPB_MediaStreamAudioTrack_API() {
  return this;
}

PP_Var MediaStreamAudioTrackResource::GetId() {
  return StringVar::StringToPPVar(id());
}

PP_Bool MediaStreamAudioTrackResource::HasEnded() {
  return PP_FromBool(has_ended());
}

int32_t MediaStreamAudioTrackResource::Configure(
    const int32_t attrib_list[],
    scoped_refptr<TrackedCallback> callback) {
  if (has_ended())
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(configure_callback_) ||
      TrackedCallback::IsPending(get_buffer_callback_)) {
    return PP_ERROR_INPROGRESS;
  }

  // Do not support configure if audio buffers are held by plugin.
  if (!buffers_.empty())
    return PP_ERROR_INPROGRESS;

  MediaStreamAudioTrackShared::Attributes attributes;
  int i = 0;
  for (; attrib_list[i] != PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE; i += 2) {
    switch (attrib_list[i]) {
      case PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERS:
        attributes.buffers = attrib_list[i + 1];
        break;
      case PP_MEDIASTREAMAUDIOTRACK_ATTRIB_DURATION:
        attributes.duration = attrib_list[i + 1];
        break;
      case PP_MEDIASTREAMAUDIOTRACK_ATTRIB_SAMPLE_RATE:
      case PP_MEDIASTREAMAUDIOTRACK_ATTRIB_SAMPLE_SIZE:
      case PP_MEDIASTREAMAUDIOTRACK_ATTRIB_CHANNELS:
        return PP_ERROR_NOTSUPPORTED;
      default:
        return PP_ERROR_BADARGUMENT;
    }
  }

  if (!MediaStreamAudioTrackShared::VerifyAttributes(attributes))
    return PP_ERROR_BADARGUMENT;

  configure_callback_ = callback;
  Call<PpapiPluginMsg_MediaStreamAudioTrack_ConfigureReply>(
      RENDERER,
      PpapiHostMsg_MediaStreamAudioTrack_Configure(attributes),
      base::Bind(&MediaStreamAudioTrackResource::OnPluginMsgConfigureReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t MediaStreamAudioTrackResource::GetAttrib(
    PP_MediaStreamAudioTrack_Attrib attrib,
    int32_t* value) {
  // TODO(penghuang): Implement this function.
  return PP_ERROR_NOTSUPPORTED;
}

int32_t MediaStreamAudioTrackResource::GetBuffer(
    PP_Resource* buffer,
    scoped_refptr<TrackedCallback> callback) {
  if (has_ended())
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(configure_callback_) ||
      TrackedCallback::IsPending(get_buffer_callback_))
    return PP_ERROR_INPROGRESS;

  *buffer = GetAudioBuffer();
  if (*buffer)
    return PP_OK;

  // TODO(penghuang): Use the callback as hints to determine which thread will
  // use the resource, so we could deliver buffers to the target thread directly
  // for better performance.
  get_buffer_output_ = buffer;
  get_buffer_callback_ = callback;
  return PP_OK_COMPLETIONPENDING;
}

int32_t MediaStreamAudioTrackResource::RecycleBuffer(PP_Resource buffer) {
  BufferMap::iterator it = buffers_.find(buffer);
  if (it == buffers_.end())
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<AudioBufferResource> buffer_resource = it->second;
  buffers_.erase(it);

  if (has_ended())
    return PP_OK;

  DCHECK_GE(buffer_resource->GetBufferIndex(), 0);

  SendEnqueueBufferMessageToHost(buffer_resource->GetBufferIndex());
  buffer_resource->Invalidate();
  return PP_OK;
}

void MediaStreamAudioTrackResource::Close() {
  if (has_ended())
    return;

  if (TrackedCallback::IsPending(get_buffer_callback_)) {
    *get_buffer_output_ = 0;
    get_buffer_callback_->PostAbort();
    get_buffer_callback_.reset();
    get_buffer_output_ = 0;
  }

  ReleaseBuffers();
  MediaStreamTrackResourceBase::CloseInternal();
}

void MediaStreamAudioTrackResource::OnNewBufferEnqueued() {
  if (!TrackedCallback::IsPending(get_buffer_callback_))
    return;

  *get_buffer_output_ = GetAudioBuffer();
  int32_t result = *get_buffer_output_ ? PP_OK : PP_ERROR_FAILED;
  get_buffer_output_ = NULL;
  scoped_refptr<TrackedCallback> callback;
  callback.swap(get_buffer_callback_);
  callback->Run(result);
}

PP_Resource MediaStreamAudioTrackResource::GetAudioBuffer() {
  int32_t index = buffer_manager()->DequeueBuffer();
  if (index < 0)
      return 0;

  MediaStreamBuffer* buffer = buffer_manager()->GetBufferPointer(index);
  DCHECK(buffer);
  scoped_refptr<AudioBufferResource> resource =
      new AudioBufferResource(pp_instance(), index, buffer);
  // Add |pp_resource()| and |resource| into |buffers_|.
  // |buffers_| uses std::unique_ptr<> to hold a ref of |resource|. It keeps the
  // resource alive.
  buffers_.insert(BufferMap::value_type(resource->pp_resource(), resource));
  return resource->GetReference();
}

void MediaStreamAudioTrackResource::ReleaseBuffers() {
  for (BufferMap::iterator it = buffers_.begin(); it != buffers_.end(); ++it) {
    // Just invalidate and release VideoBufferResorce, but keep PP_Resource.
    // So plugin can still use |RecycleBuffer()|.
    it->second->Invalidate();
    it->second.reset();
  }
}

void MediaStreamAudioTrackResource::OnPluginMsgConfigureReply(
    const ResourceMessageReplyParams& params) {
  if (TrackedCallback::IsPending(configure_callback_)) {
    scoped_refptr<TrackedCallback> callback;
    callback.swap(configure_callback_);
    callback->Run(params.result());
  }
}

}  // namespace proxy
}  // namespace ppapi
