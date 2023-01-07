// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/dictionary_var.h"

#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

DictionaryVar::DictionaryVar() {}

DictionaryVar::~DictionaryVar() {}

// static
DictionaryVar* DictionaryVar::FromPPVar(const PP_Var& var) {
  if (var.type != PP_VARTYPE_DICTIONARY)
    return NULL;

  scoped_refptr<Var> var_object(
      PpapiGlobals::Get()->GetVarTracker()->GetVar(var));
  if (!var_object.get())
    return NULL;
  return var_object->AsDictionaryVar();
}

DictionaryVar* DictionaryVar::AsDictionaryVar() { return this; }

PP_VarType DictionaryVar::GetType() const { return PP_VARTYPE_DICTIONARY; }

PP_Var DictionaryVar::Get(const PP_Var& key) const {
  StringVar* string_var = StringVar::FromPPVar(key);
  if (!string_var)
    return PP_MakeUndefined();

  KeyValueMap::const_iterator iter = key_value_map_.find(string_var->value());
  if (iter != key_value_map_.end()) {
    if (PpapiGlobals::Get()->GetVarTracker()->AddRefVar(iter->second.get()))
      return iter->second.get();
    else
      return PP_MakeUndefined();
  } else {
    return PP_MakeUndefined();
  }
}

PP_Bool DictionaryVar::Set(const PP_Var& key, const PP_Var& value) {
  StringVar* string_var = StringVar::FromPPVar(key);
  if (!string_var)
    return PP_FALSE;

  key_value_map_[string_var->value()] = value;
  return PP_TRUE;
}

void DictionaryVar::Delete(const PP_Var& key) {
  StringVar* string_var = StringVar::FromPPVar(key);
  if (!string_var)
    return;

  key_value_map_.erase(string_var->value());
}

PP_Bool DictionaryVar::HasKey(const PP_Var& key) const {
  StringVar* string_var = StringVar::FromPPVar(key);
  if (!string_var)
    return PP_FALSE;

  bool result =
      key_value_map_.find(string_var->value()) != key_value_map_.end();
  return PP_FromBool(result);
}

PP_Var DictionaryVar::GetKeys() const {
  scoped_refptr<ArrayVar> array_var(new ArrayVar());
  array_var->elements().reserve(key_value_map_.size());

  for (KeyValueMap::const_iterator iter = key_value_map_.begin();
       iter != key_value_map_.end();
       ++iter) {
    array_var->elements().push_back(ScopedPPVar(
        ScopedPPVar::PassRef(), StringVar::StringToPPVar(iter->first)));
  }
  return array_var->GetPPVar();
}

bool DictionaryVar::SetWithStringKey(const std::string& utf8_key,
                                     const PP_Var& value) {
  if (!base::IsStringUTF8(utf8_key))
    return false;

  key_value_map_[utf8_key] = value;
  return true;
}

void DictionaryVar::DeleteWithStringKey(const std::string& utf8_key) {
  key_value_map_.erase(utf8_key);
}

}  // namespace ppapi
