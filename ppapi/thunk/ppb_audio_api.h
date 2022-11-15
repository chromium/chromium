// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_AUDIO_API_H_
#define PPAPI_THUNK_PPB_AUDIO_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace base {
class UnsafeSharedMemoryRegion;
}  // namespace base

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Audio_API {
 public:
  virtual ~PPB_Audio_API() {}

  virtual PP_Resource GetCurrentConfig() = 0;
  virtual PP_Bool StartPlayback() = 0;
  virtual PP_Bool StopPlayback() = 0;

  // Trusted API.
  virtual int32_t Open(
      PP_Resource config_id,
      scoped_refptr<TrackedCallback> create_callback) = 0;
  virtual int32_t GetSyncSocket(int* sync_socket) = 0;
  virtual int32_t GetSharedMemory(base::UnsafeSharedMemoryRegion** shm) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_AUDIO_API_H_
