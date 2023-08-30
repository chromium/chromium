// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_AUDIO_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_AUDIO_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "content/renderer/pepper/audio_helper.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"

namespace content {
class PepperPlatformAudioOutput;

// Some of the backend functionality of this class is implemented by the
// PPB_Audio_Shared so it can be shared with the proxy.
//
// TODO(teravest): PPB_Audio is no longer supported in-process. Clean this up
// to look more like typical HostResource implementations.
class PPB_Audio_Impl : public ppapi::Resource,
                       public ppapi::PPB_Audio_Shared,
                       public AudioHelper {
 public:
  explicit PPB_Audio_Impl(PP_Instance instance);

  PPB_Audio_Impl(const PPB_Audio_Impl&) = delete;
  PPB_Audio_Impl& operator=(const PPB_Audio_Impl&) = delete;

  // Resource overrides.
  ppapi::thunk::PPB_Audio_API* AsPPB_Audio_API() override;

  // PPB_Audio_API implementation.
  PP_Resource GetCurrentConfig() override;
  PP_Bool StartPlayback() override;
  PP_Bool StopPlayback() override;
  int32_t Open(PP_Resource config_id,
               scoped_refptr<ppapi::TrackedCallback> create_callback) override;
  int32_t GetSyncSocket(int* sync_socket) override;
  int32_t GetSharedMemory(base::UnsafeSharedMemoryRegion** shm) override;

  void SetVolume(double volume);

 private:
  ~PPB_Audio_Impl() override;

  // AudioHelper implementation.
  void OnSetStreamInfo(base::UnsafeSharedMemoryRegion shared_memory_region,
                       base::SyncSocket::ScopedHandle socket) override;

  // AudioConfig used for creating this Audio object. We own a ref.
  ppapi::ScopedPPResource config_;

  // PluginDelegate audio object that we delegate audio IPC through. We don't
  // own this pointer but are responsible for calling Shutdown on it.
  raw_ptr<PepperPlatformAudioOutput> audio_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_AUDIO_IMPL_H_
