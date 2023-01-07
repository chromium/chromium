// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_MEDIA_STREAM_TRACK_RESOURCE_BASE_H_
#define PPAPI_PROXY_MEDIA_STREAM_TRACK_RESOURCE_BASE_H_

#include <stdint.h>

#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/media_stream_buffer_manager.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT MediaStreamTrackResourceBase
  : public PluginResource,
    public MediaStreamBufferManager::Delegate {
 protected:
  MediaStreamTrackResourceBase(Connection connection,
                               PP_Instance instance,
                               int pending_renderer_id,
                               const std::string& id);

  MediaStreamTrackResourceBase(Connection connection, PP_Instance instance);

  MediaStreamTrackResourceBase(const MediaStreamTrackResourceBase&) = delete;
  MediaStreamTrackResourceBase& operator=(const MediaStreamTrackResourceBase&) =
      delete;

  ~MediaStreamTrackResourceBase() override;

  std::string id() const { return id_; }

  void set_id(const std::string& id) { id_ = id; }

  bool has_ended() const { return has_ended_; }

  MediaStreamBufferManager* buffer_manager() { return &buffer_manager_; }

  void CloseInternal();

  // Sends a buffer index to the corresponding PepperMediaStreamTrackHostBase
  // via an IPC message. The host adds the buffer index into its
  // |buffer_manager_| for reading or writing.
  // Also see |MediaStreamBufferManager|.
  void SendEnqueueBufferMessageToHost(int32_t index);

  // PluginResource overrides:
  void OnReplyReceived(const ResourceMessageReplyParams& params,
                       const IPC::Message& msg) override;

 private:
  // Message handlers:
  void OnPluginMsgInitBuffers(const ResourceMessageReplyParams& params,
                              int32_t number_of_buffers,
                              int32_t buffer_size,
                              bool readonly);
  void OnPluginMsgEnqueueBuffer(const ResourceMessageReplyParams& params,
                                int32_t index);
  void OnPluginMsgEnqueueBuffers(const ResourceMessageReplyParams& params,
                                 const std::vector<int32_t>& indices);

  MediaStreamBufferManager buffer_manager_;

  std::string id_;

  bool has_ended_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_MEDIA_STREAM_TRACK_RESOURCE_BASE_H_
