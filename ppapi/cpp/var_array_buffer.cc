// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/var_array_buffer.h"

#include <limits>

#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VarArrayBuffer_1_0>() {
  return PPB_VAR_ARRAY_BUFFER_INTERFACE_1_0;
}

}  // namespace

VarArrayBuffer::VarArrayBuffer() {
  ConstructWithSize(0);
}

VarArrayBuffer::VarArrayBuffer(const Var& var) : Var(var) {
  if (!var.is_array_buffer()) {
    PP_NOTREACHED();
    var_ = PP_MakeNull();
  }
}

VarArrayBuffer::VarArrayBuffer(uint32_t size_in_bytes) {
  ConstructWithSize(size_in_bytes);
}

pp::VarArrayBuffer& VarArrayBuffer::operator=(const VarArrayBuffer& other) {
  Var::operator=(other);
  return *this;
}

pp::Var& VarArrayBuffer::operator=(const Var& other) {
  if (other.is_array_buffer()) {
    return Var::operator=(other);
  } else {
    PP_NOTREACHED();
    return *this;
  }
}

uint32_t VarArrayBuffer::ByteLength() const {
  uint32_t byte_length = std::numeric_limits<uint32_t>::max();
  if (is_array_buffer() && has_interface<PPB_VarArrayBuffer_1_0>())
    get_interface<PPB_VarArrayBuffer_1_0>()->ByteLength(var_, &byte_length);
  else
    PP_NOTREACHED();
  return byte_length;
}

void* VarArrayBuffer::Map() {
  if (is_array_buffer() && has_interface<PPB_VarArrayBuffer_1_0>())
    return get_interface<PPB_VarArrayBuffer_1_0>()->Map(var_);
  PP_NOTREACHED();
  return NULL;
}

void VarArrayBuffer::Unmap() {
  if (is_array_buffer() && has_interface<PPB_VarArrayBuffer_1_0>())
    get_interface<PPB_VarArrayBuffer_1_0>()->Unmap(var_);
  else
    PP_NOTREACHED();
}


void VarArrayBuffer::ConstructWithSize(uint32_t size_in_bytes) {
  PP_DCHECK(is_undefined());

  if (has_interface<PPB_VarArrayBuffer_1_0>()) {
    var_ = get_interface<PPB_VarArrayBuffer_1_0>()->Create(size_in_bytes);
  } else {
    PP_NOTREACHED();
    var_ = PP_MakeNull();
  }
  is_managed_ = true;
}

}  // namespace pp
