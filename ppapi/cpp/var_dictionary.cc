// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/var_dictionary.h"

#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VarDictionary_1_0>() {
  return PPB_VAR_DICTIONARY_INTERFACE_1_0;
}

}  // namespace

VarDictionary::VarDictionary() : Var(Null()) {
  if (has_interface<PPB_VarDictionary_1_0>())
    var_ = get_interface<PPB_VarDictionary_1_0>()->Create();
  else
    PP_NOTREACHED();
}

VarDictionary::VarDictionary(const Var& var) : Var(var) {
  if (!var.is_dictionary()) {
    PP_NOTREACHED();

    // This takes care of releasing the reference that this object holds.
    Var::operator=(Var(Null()));
  }
}

VarDictionary::VarDictionary(const PP_Var& var) : Var(var) {
  if (var.type != PP_VARTYPE_DICTIONARY) {
    PP_NOTREACHED();

    // This takes care of releasing the reference that this object holds.
    Var::operator=(Var(Null()));
  }
}

VarDictionary::VarDictionary(const VarDictionary& other)
    : Var(other) {
}

VarDictionary::~VarDictionary() {
}

VarDictionary& VarDictionary::operator=(
    const VarDictionary& other) {
  Var::operator=(other);
  return *this;
}

Var& VarDictionary::operator=(const Var& other) {
  if (other.is_dictionary()) {
    Var::operator=(other);
  } else {
    PP_NOTREACHED();
    Var::operator=(Var(Null()));
  }
  return *this;
}

Var VarDictionary::Get(const Var& key) const {
  if (!has_interface<PPB_VarDictionary_1_0>())
    return Var();

  return Var(
      PASS_REF,
      get_interface<PPB_VarDictionary_1_0>()->Get(var_, key.pp_var()));
}

bool VarDictionary::Set(const Var& key, const Var& value) {
  if (!has_interface<PPB_VarDictionary_1_0>())
    return false;

  return PP_ToBool(get_interface<PPB_VarDictionary_1_0>()->Set(
      var_, key.pp_var(), value.pp_var()));
}

void VarDictionary::Delete(const Var& key) {
  if (has_interface<PPB_VarDictionary_1_0>())
    get_interface<PPB_VarDictionary_1_0>()->Delete(var_, key.pp_var());
}

bool VarDictionary::HasKey(const Var& key) const {
  if (!has_interface<PPB_VarDictionary_1_0>())
    return false;

  return PP_ToBool(get_interface<PPB_VarDictionary_1_0>()->HasKey(
      var_, key.pp_var()));
}

VarArray VarDictionary::GetKeys() const {
  if (!has_interface<PPB_VarDictionary_1_0>())
    return VarArray();

  Var result(PASS_REF,
             get_interface<PPB_VarDictionary_1_0>()->GetKeys(var_));
  if (result.is_array())
    return VarArray(result);
  else
    return VarArray();
}

}  // namespace pp
