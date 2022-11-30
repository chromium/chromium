// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_AUDIO_HELPER_H_
#define CONTENT_RENDERER_PEPPER_AUDIO_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace content {

class AudioHelper {
 public:
  AudioHelper();

  AudioHelper(const AudioHelper&) = delete;
  AudioHelper& operator=(const AudioHelper&) = delete;

  virtual ~AudioHelper();

  // Called when the stream is created.
  void StreamCreated(base::UnsafeSharedMemoryRegion shared_memory_region,
                     base::SyncSocket::ScopedHandle socket);

  void SetCreateCallback(scoped_refptr<ppapi::TrackedCallback> create_callback);

 protected:
  // TODO(viettrungluu): This is all very poorly thought out. Refactor.

  // To be called by implementations of |PPB_Audio_API|/|PPB_AudioInput_API|.
  int32_t GetSyncSocketImpl(int* sync_socket);
  int32_t GetSharedMemoryImpl(base::UnsafeSharedMemoryRegion** shm);

  // To be implemented by subclasses to call their |SetStreamInfo()|.
  virtual void OnSetStreamInfo(
      base::UnsafeSharedMemoryRegion shared_memory_region,
      base::SyncSocket::ScopedHandle socket_handle) = 0;

 private:
  scoped_refptr<ppapi::TrackedCallback> create_callback_;

  // When a create callback is being issued, these will save the info for
  // querying from the callback. The proxy uses this to get the handles to the
  // other process instead of mapping them in the renderer. These will be
  // invalid all other times.
  base::UnsafeSharedMemoryRegion shared_memory_for_create_callback_;
  std::unique_ptr<base::SyncSocket> socket_for_create_callback_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_AUDIO_HELPER_H_
