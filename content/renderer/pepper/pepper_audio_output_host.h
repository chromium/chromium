// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_OUTPUT_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_OUTPUT_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {
class PepperPlatformAudioOutputDev;
class RendererPpapiHostImpl;

class PepperAudioOutputHost : public ppapi::host::ResourceHost {
 public:
  PepperAudioOutputHost(RendererPpapiHostImpl* host,
                        PP_Instance instance,
                        PP_Resource resource);

  PepperAudioOutputHost(const PepperAudioOutputHost&) = delete;
  PepperAudioOutputHost& operator=(const PepperAudioOutputHost&) = delete;

  ~PepperAudioOutputHost() override;

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // Called when the stream is created.
  void StreamCreated(base::UnsafeSharedMemoryRegion shared_memory_region,
                     base::SyncSocket::ScopedHandle socket);
  void StreamCreationFailed();
  void SetVolume(double volume);

 private:
  int32_t OnOpen(ppapi::host::HostMessageContext* context,
                 const std::string& device_id,
                 PP_AudioSampleRate sample_rate,
                 uint32_t sample_frame_count);
  int32_t OnStartOrStop(ppapi::host::HostMessageContext* context,
                        bool playback);
  int32_t OnClose(ppapi::host::HostMessageContext* context);

  void OnOpenComplete(int32_t result,
                      base::UnsafeSharedMemoryRegion shared_memory_region,
                      base::SyncSocket::ScopedHandle socket_handle);

  int32_t GetRemoteHandles(
      const base::SyncSocket& socket,
      const base::UnsafeSharedMemoryRegion& shared_memory_region,
      IPC::PlatformFileForTransit* remote_socket_handle,
      base::UnsafeSharedMemoryRegion* remote_shared_memory_region);

  void Close();

  void SendOpenReply(int32_t result);

  // Non-owning pointer.
  raw_ptr<RendererPpapiHostImpl> renderer_ppapi_host_;

  ppapi::host::ReplyMessageContext open_context_;

  // Audio output object that we delegate audio IPC through.
  // We don't own this pointer but are responsible for calling Shutdown on it.
  raw_ptr<PepperPlatformAudioOutputDev> audio_output_;

  PepperDeviceEnumerationHostHelper enumeration_helper_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_OUTPUT_HOST_H_
