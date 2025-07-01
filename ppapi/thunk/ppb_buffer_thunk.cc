// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance, uint32_t size) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateBuffer(instance, size);
}

PP_Bool IsBuffer(PP_Resource resource) {
  EnterResource<PPB_Buffer_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool Describe(PP_Resource resource, uint32_t* size_in_bytes) {
  EnterResource<PPB_Buffer_API> enter(resource, true);
  if (enter.failed()) {
    *size_in_bytes = 0;
    return PP_FALSE;
  }
  return enter.object()->Describe(size_in_bytes);
}

void* Map(PP_Resource resource) {
  EnterResource<PPB_Buffer_API> enter(resource, true);
  if (enter.failed())
    return NULL;
  return enter.object()->Map();
}

void Unmap(PP_Resource resource) {
  EnterResource<PPB_Buffer_API> enter(resource, true);
  if (enter.succeeded())
    enter.object()->Unmap();
}

const PPB_Buffer_Dev g_ppb_buffer_thunk = {
  &Create,
  &IsBuffer,
  &Describe,
  &Map,
  &Unmap,
};

}  // namespace

const PPB_Buffer_Dev_0_4* GetPPB_Buffer_Dev_0_4_Thunk() {
  return &g_ppb_buffer_thunk;
}

}  // namespace thunk
}  // namespace ppapi
