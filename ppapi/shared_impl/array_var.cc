// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/array_var.h"

#include <limits>

#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

ArrayVar::ArrayVar() {}

ArrayVar::~ArrayVar() {}

// static
ArrayVar* ArrayVar::FromPPVar(const PP_Var& var) {
  if (var.type != PP_VARTYPE_ARRAY)
    return NULL;

  scoped_refptr<Var> var_object(
      PpapiGlobals::Get()->GetVarTracker()->GetVar(var));
  if (!var_object.get())
    return NULL;
  return var_object->AsArrayVar();
}

ArrayVar* ArrayVar::AsArrayVar() { return this; }

PP_VarType ArrayVar::GetType() const { return PP_VARTYPE_ARRAY; }

PP_Var ArrayVar::Get(uint32_t index) const {
  if (index >= elements_.size())
    return PP_MakeUndefined();

  const PP_Var& element = elements_[index].get();
  if (PpapiGlobals::Get()->GetVarTracker()->AddRefVar(element))
    return element;
  else
    return PP_MakeUndefined();
}

PP_Bool ArrayVar::Set(uint32_t index, const PP_Var& value) {
  if (index == std::numeric_limits<uint32_t>::max())
    return PP_FALSE;

  if (index >= elements_.size()) {
    // Insert ScopedPPVars of type PP_VARTYPE_UNDEFINED to reach the new size
    // (index + 1).
    elements_.resize(index + 1);
  }

  elements_[index] = value;
  return PP_TRUE;
}

uint32_t ArrayVar::GetLength() const {
  if (elements_.size() > std::numeric_limits<uint32_t>::max()) {
    CHECK(false);
    return 0;
  }

  return static_cast<uint32_t>(elements_.size());
}

PP_Bool ArrayVar::SetLength(uint32_t length) {
  // If |length| is larger than the current size, ScopedPPVars of type
  // PP_VARTYPE_UNDEFINED will be inserted to reach the new length.
  elements_.resize(length);
  return PP_TRUE;
}

}  // namespace ppapi
