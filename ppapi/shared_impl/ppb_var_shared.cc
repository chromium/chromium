// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_var_shared.h"

#include <limits>

#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

using ppapi::PpapiGlobals;
using ppapi::StringVar;

namespace ppapi {
namespace {

// PPB_Var methods -------------------------------------------------------------

void AddRefVar(PP_Var var) {
  ProxyAutoLock lock;
  PpapiGlobals::Get()->GetVarTracker()->AddRefVar(var);
}

void ReleaseVar(PP_Var var) {
  ProxyAutoLock lock;
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var);
}

PP_Var VarFromUtf8(const char* data, uint32_t len) {
  ProxyAutoLock lock;
  return StringVar::StringToPPVar(data, len);
}

PP_Var VarFromUtf8_1_0(PP_Module /*module*/, const char* data, uint32_t len) {
  return VarFromUtf8(data, len);
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  ProxyAutoLock lock;
  StringVar* str = StringVar::FromPPVar(var);
  if (str) {
    *len = static_cast<uint32_t>(str->value().size());
    return str->value().c_str();
  }
  *len = 0;
  return NULL;
}

PP_Resource VarToResource(PP_Var var) {
  ProxyAutoLock lock;
  ResourceVar* resource = ResourceVar::FromPPVar(var);
  if (!resource)
    return 0;
  PP_Resource pp_resource = resource->GetPPResource();
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(pp_resource);
  return pp_resource;
}

PP_Var VarFromResource(PP_Resource resource) {
  ProxyAutoLock lock;
  return PpapiGlobals::Get()->GetVarTracker()->MakeResourcePPVar(resource);
}

const PPB_Var var_interface = {&AddRefVar, &ReleaseVar,    &VarFromUtf8,
                               &VarToUtf8, &VarToResource, &VarFromResource};

const PPB_Var_1_1 var_interface1_1 = {&AddRefVar,   &ReleaseVar,
                                      &VarFromUtf8, &VarToUtf8};

const PPB_Var_1_0 var_interface1_0 = {&AddRefVar,       &ReleaseVar,
                                      &VarFromUtf8_1_0, &VarToUtf8};

// PPB_VarArrayBuffer methods --------------------------------------------------

PP_Var CreateArrayBufferVar(uint32_t size_in_bytes) {
  ProxyAutoLock lock;
  return PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
      size_in_bytes);
}

PP_Bool ByteLength(PP_Var array, uint32_t* byte_length) {
  ProxyAutoLock lock;
  ArrayBufferVar* buffer = ArrayBufferVar::FromPPVar(array);
  if (!buffer)
    return PP_FALSE;
  *byte_length = buffer->ByteLength();
  return PP_TRUE;
}

void* Map(PP_Var array) {
  ProxyAutoLock lock;
  ArrayBufferVar* buffer = ArrayBufferVar::FromPPVar(array);
  if (!buffer)
    return NULL;
  return buffer->Map();
}

void Unmap(PP_Var array) {
  ProxyAutoLock lock;
  ArrayBufferVar* buffer = ArrayBufferVar::FromPPVar(array);
  if (buffer)
    buffer->Unmap();
}

const PPB_VarArrayBuffer_1_0 var_arraybuffer_interface = {
    &CreateArrayBufferVar, &ByteLength, &Map, &Unmap};

}  // namespace

// static
const PPB_Var_1_2* PPB_Var_Shared::GetVarInterface1_2() {
  return &var_interface;
}

// static
const PPB_Var_1_1* PPB_Var_Shared::GetVarInterface1_1() {
  return &var_interface1_1;
}

// static
const PPB_Var_1_0* PPB_Var_Shared::GetVarInterface1_0() {
  return &var_interface1_0;
}

// static
const PPB_VarArrayBuffer_1_0* PPB_Var_Shared::GetVarArrayBufferInterface1_0() {
  return &var_arraybuffer_interface;
}

}  // namespace ppapi
