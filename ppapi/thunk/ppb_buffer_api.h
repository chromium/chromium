// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_BUFFER_API_H_
#define PPAPI_THUNK_PPB_BUFFER_API_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace base {
class UnsafeSharedMemoryRegion;
}  // namespace base

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Buffer_API {
 public:
  virtual ~PPB_Buffer_API() {}

  virtual PP_Bool Describe(uint32_t* size_in_bytes) = 0;
  virtual PP_Bool IsMapped() = 0;
  virtual void* Map() = 0;
  virtual void Unmap() = 0;

  // Trusted API
  virtual int32_t GetSharedMemory(base::UnsafeSharedMemoryRegion** shm) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_BUFFER_API_H_
