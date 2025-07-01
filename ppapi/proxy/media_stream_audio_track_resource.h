// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_MEDIA_STREAM_AUDIO_TRACK_RESOURCE_H_
#define PPAPI_PROXY_MEDIA_STREAM_AUDIO_TRACK_RESOURCE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "ppapi/proxy/media_stream_track_resource_base.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_media_stream_audio_track_api.h"

namespace ppapi {
namespace proxy {

class AudioBufferResource;

class PPAPI_PROXY_EXPORT MediaStreamAudioTrackResource
    : public MediaStreamTrackResourceBase,
      public thunk::PPB_MediaStreamAudioTrack_API {
 public:
  MediaStreamAudioTrackResource(Connection connection,
                                PP_Instance instance,
                                int pending_renderer_id,
                                const std::string& id);

  MediaStreamAudioTrackResource(const MediaStreamAudioTrackResource&) = delete;
  MediaStreamAudioTrackResource& operator=(
      const MediaStreamAudioTrackResource&) = delete;

  ~MediaStreamAudioTrackResource() override;

  // Resource overrides:
  thunk::PPB_MediaStreamAudioTrack_API* AsPPB_MediaStreamAudioTrack_API()
      override;

  // PPB_MediaStreamAudioTrack_API overrides:
  PP_Var GetId() override;
  PP_Bool HasEnded() override;
  int32_t Configure(const int32_t attrib_list[],
                    scoped_refptr<TrackedCallback> callback) override;
  int32_t GetAttrib(PP_MediaStreamAudioTrack_Attrib attrib,
                    int32_t* value) override;
  int32_t GetBuffer(PP_Resource* buffer,
                    scoped_refptr<TrackedCallback> callback) override;
  int32_t RecycleBuffer(PP_Resource buffer) override;
  void Close() override;

  // MediaStreamBufferManager::Delegate overrides:
  void OnNewBufferEnqueued() override;

 private:
  PP_Resource GetAudioBuffer();

  void ReleaseBuffers();

  // IPC message handlers.
  void OnPluginMsgConfigureReply(const ResourceMessageReplyParams& params);

  // Allocated buffer resources by |GetBuffer()|.
  typedef std::map<PP_Resource, scoped_refptr<AudioBufferResource> > BufferMap;
  BufferMap buffers_;

  PP_Resource* get_buffer_output_;

  scoped_refptr<TrackedCallback> configure_callback_;

  scoped_refptr<TrackedCallback> get_buffer_callback_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_MEDIA_STREAM_AUDIO_TRACK_RESOURCE_H_
