// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>

#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

// The memory interface doesn't have a normal C -> C++ thunk since it doesn't
// actually have any proxy wrapping or associated objects; it's just a call
// into base. So we implement the entire interface here, using the thunk
// namespace so it magically gets hooked up in the proper places.

namespace ppapi {

namespace {

void* MemAlloc(uint32_t num_bytes) { return malloc(num_bytes); }

void MemFree(void* ptr) { free(ptr); }

const PPB_Memory_Dev ppb_memory = {&MemAlloc, &MemFree};

}  // namespace

namespace thunk {

// static
PPAPI_SHARED_EXPORT const PPB_Memory_Dev_0_1* GetPPB_Memory_Dev_0_1_Thunk() {
  return &ppb_memory;
}

}  // namespace thunk

}  // namespace ppapi
