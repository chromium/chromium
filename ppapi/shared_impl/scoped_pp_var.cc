// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/shared_impl/scoped_pp_var.h"

#include <stdint.h>

#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {

namespace {

void CallAddRef(const PP_Var& v) {
  PpapiGlobals::Get()->GetVarTracker()->AddRefVar(v);
}

void CallRelease(const PP_Var& v) {
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(v);
}

}  // namespace

ScopedPPVar::ScopedPPVar() : var_(PP_MakeUndefined()) {}

ScopedPPVar::ScopedPPVar(const PP_Var& v) : var_(v) { CallAddRef(var_); }

ScopedPPVar::ScopedPPVar(const PassRef&, const PP_Var& v) : var_(v) {}

ScopedPPVar::ScopedPPVar(const ScopedPPVar& other) : var_(other.var_) {
  CallAddRef(var_);
}

ScopedPPVar::~ScopedPPVar() { CallRelease(var_); }

ScopedPPVar& ScopedPPVar::operator=(const PP_Var& v) {
  CallAddRef(v);
  CallRelease(var_);
  var_ = v;
  return *this;
}

PP_Var ScopedPPVar::Release() {
  PP_Var result = var_;
  var_ = PP_MakeUndefined();
  return result;
}

ScopedPPVarArray::ScopedPPVarArray(const PassPPBMemoryAllocatedArray&,
                                   PP_Var* array,
                                   size_t size)
    : array_(array),
      size_(size) {}

ScopedPPVarArray::ScopedPPVarArray(size_t size)
    : size_(size) {
  if (size > 0) {
    array_ = static_cast<PP_Var*>(
        thunk::GetPPB_Memory_Dev_0_1_Thunk()->MemAlloc(
            static_cast<uint32_t>(sizeof(PP_Var) * size)));
  }
  for (size_t i = 0; i < size_; ++i)
    array_[i] = PP_MakeUndefined();
}

ScopedPPVarArray::~ScopedPPVarArray() {
  for (size_t i = 0; i < size_; ++i)
    CallRelease(array_[i]);
  if (size_ > 0)
    thunk::GetPPB_Memory_Dev_0_1_Thunk()->MemFree(array_);

}

PP_Var* ScopedPPVarArray::Release(const PassPPBMemoryAllocatedArray&) {
  PP_Var* result = array_;
  array_ = NULL;
  size_ = 0;
  return result;
}

void ScopedPPVarArray::Set(size_t index, const ScopedPPVar& var) {
  DCHECK(index < size_);
  CallAddRef(var.get());
  CallRelease(array_[index]);
  array_[index] = var.get();
}

}  // namespace ppapi
