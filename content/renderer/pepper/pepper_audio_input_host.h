// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_INPUT_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_INPUT_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sync_socket.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {
class PepperPlatformAudioInput;
class RendererPpapiHostImpl;

class PepperAudioInputHost : public ppapi::host::ResourceHost {
 public:
  PepperAudioInputHost(RendererPpapiHostImpl* host,
                       PP_Instance instance,
                       PP_Resource resource);
  ~PepperAudioInputHost() override;

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // Called when the stream is created.
  void StreamCreated(base::ReadOnlySharedMemoryRegion shared_memory_region,
                     base::SyncSocket::ScopedHandle socket);
  void StreamCreationFailed();

 private:
  int32_t OnOpen(ppapi::host::HostMessageContext* context,
                 const std::string& device_id,
                 PP_AudioSampleRate sample_rate,
                 uint32_t sample_frame_count);
  int32_t OnStartOrStop(ppapi::host::HostMessageContext* context, bool capture);
  int32_t OnClose(ppapi::host::HostMessageContext* context);

  void OnOpenComplete(int32_t result,
                      base::ReadOnlySharedMemoryRegion shared_memory_region,
                      base::SyncSocket::ScopedHandle socket_handle);

  int32_t GetRemoteHandles(
      const base::SyncSocket& socket,
      const base::ReadOnlySharedMemoryRegion& shared_memory_region,
      IPC::PlatformFileForTransit* remote_socket_handle,
      base::ReadOnlySharedMemoryRegion* remote_shared_memory_region);

  void Close();

  void SendOpenReply(int32_t result);

  // Non-owning pointer.
  RendererPpapiHostImpl* renderer_ppapi_host_;

  ppapi::host::ReplyMessageContext open_context_;

  // Audio input object that we delegate audio IPC through.
  // We don't own this pointer but are responsible for calling Shutdown on it.
  PepperPlatformAudioInput* audio_input_;

  PepperDeviceEnumerationHostHelper enumeration_helper_;

  DISALLOW_COPY_AND_ASSIGN(PepperAudioInputHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_INPUT_HOST_H_
