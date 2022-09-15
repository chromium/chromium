// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/var_array.h"

#include "ppapi/c/ppb_var_array.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VarArray_1_0>() {
  return PPB_VAR_ARRAY_INTERFACE_1_0;
}

}  // namespace

VarArray::VarArray() : Var(Null()) {
  if (has_interface<PPB_VarArray_1_0>())
    var_ = get_interface<PPB_VarArray_1_0>()->Create();
  else
    PP_NOTREACHED();
}

VarArray::VarArray(const Var& var) : Var(var) {
  if (!var.is_array()) {
    PP_NOTREACHED();

    // This takes care of releasing the reference that this object holds.
    Var::operator=(Var(Null()));
  }
}

VarArray::VarArray(const PP_Var& var) : Var(var) {
  if (var.type != PP_VARTYPE_ARRAY) {
    PP_NOTREACHED();

    // This takes care of releasing the reference that this object holds.
    Var::operator=(Var(Null()));
  }
}

VarArray::VarArray(const VarArray& other) : Var(other) {
}

VarArray::~VarArray() {
}

VarArray& VarArray::operator=(const VarArray& other) {
  Var::operator=(other);
  return *this;
}

Var& VarArray::operator=(const Var& other) {
  if (other.is_array()) {
    Var::operator=(other);
  } else {
    PP_NOTREACHED();
    Var::operator=(Var(Null()));
  }
  return *this;
}

Var VarArray::Get(uint32_t index) const {
  if (!has_interface<PPB_VarArray_1_0>())
    return Var();

  return Var(PASS_REF, get_interface<PPB_VarArray_1_0>()->Get(var_, index));
}

bool VarArray::Set(uint32_t index, const Var& value) {
  if (!has_interface<PPB_VarArray_1_0>())
    return false;

  return PP_ToBool(get_interface<PPB_VarArray_1_0>()->Set(var_, index,
                                                          value.pp_var()));
}

uint32_t VarArray::GetLength() const {
  if (!has_interface<PPB_VarArray_1_0>())
    return 0;

  return get_interface<PPB_VarArray_1_0>()->GetLength(var_);
}

bool VarArray::SetLength(uint32_t length) {
  if (!has_interface<PPB_VarArray_1_0>())
    return false;

  return PP_ToBool(get_interface<PPB_VarArray_1_0>()->SetLength(var_, length));
}

}  // namespace pp
