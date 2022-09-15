// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_AUDIO_PROXY_H_
#define PPAPI_PROXY_PPB_AUDIO_PROXY_H_

#include <stdint.h>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_completion_callback_factory.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace ppapi {

class AudioCallbackCombined;
class HostResource;

namespace proxy {

class SerializedHandle;

class PPB_Audio_Proxy : public InterfaceProxy {
 public:
  explicit PPB_Audio_Proxy(Dispatcher* dispatcher);

  PPB_Audio_Proxy(const PPB_Audio_Proxy&) = delete;
  PPB_Audio_Proxy& operator=(const PPB_Audio_Proxy&) = delete;

  ~PPB_Audio_Proxy() override;

  // Creates an Audio object in the plugin process.
  static PP_Resource CreateProxyResource(
      PP_Instance instance_id,
      PP_Resource config_id,
      const AudioCallbackCombined& audio_callback,
      void* user_data);

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  static const ApiID kApiID = API_ID_PPB_AUDIO;

 private:
  // Plugin->renderer message handlers.
  void OnMsgCreate(PP_Instance instance_id,
                   int32_t sample_rate,
                   uint32_t sample_frame_count,
                   ppapi::HostResource* result);
  void OnMsgStartOrStop(const ppapi::HostResource& audio_id, bool play);

  // Renderer->plugin message handlers.
  void OnMsgNotifyAudioStreamCreated(
      const ppapi::HostResource& audio_id,
      int32_t result_code,
      ppapi::proxy::SerializedHandle socket_handle,
      ppapi::proxy::SerializedHandle handle);

  void AudioChannelConnected(int32_t result,
                             const ppapi::HostResource& resource);

  // In the renderer, this is called in response to a stream created message.
  // It will retrieve the shared memory and socket handles and place them into
  // the given out params. The return value is a PPAPI error code.
  //
  // The input arguments should be initialized to 0 or -1, depending on the
  // platform's default invalid handle values. On error, some of these
  // arguments may be written to, and others may be untouched, depending on
  // where the error occurred.
  int32_t GetAudioConnectedHandles(
      const ppapi::HostResource& resource,
      IPC::PlatformFileForTransit* foreign_socket_handle,
      base::UnsafeSharedMemoryRegion* foreign_shared_memory_region);

  ProxyCompletionCallbackFactory<PPB_Audio_Proxy> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_AUDIO_PROXY_H_
