// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_DICTIONARY_VAR_H_
#define PPAPI_SHARED_IMPL_DICTIONARY_VAR_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT DictionaryVar : public Var {
 public:
  typedef std::map<std::string, ScopedPPVar> KeyValueMap;

  DictionaryVar();

  DictionaryVar(const DictionaryVar&) = delete;
  DictionaryVar& operator=(const DictionaryVar&) = delete;

  // Helper function that converts a PP_Var to a DictionaryVar. This will
  // return NULL if the PP_Var is not of type PP_VARTYPE_DICTIONARY or the
  // dictionary cannot be found from the var tracker.
  static DictionaryVar* FromPPVar(const PP_Var& var);

  // Var overrides.
  DictionaryVar* AsDictionaryVar() override;
  PP_VarType GetType() const override;

  // The returned PP_Var has had a ref added on behalf of the caller.
  PP_Var Get(const PP_Var& key) const;
  PP_Bool Set(const PP_Var& key, const PP_Var& value);
  void Delete(const PP_Var& key);
  PP_Bool HasKey(const PP_Var& key) const;
  // The returned PP_Var has had a ref added on behalf of the caller.
  PP_Var GetKeys() const;

  // Returns false and keeps the dictionary unchanged if |key| is not a valid
  // UTF-8 string.
  bool SetWithStringKey(const std::string& utf8_key, const PP_Var& value);
  void DeleteWithStringKey(const std::string& utf8_key);

  const KeyValueMap& key_value_map() const { return key_value_map_; }

 protected:
  ~DictionaryVar() override;

 private:
  KeyValueMap key_value_map_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_DICTIONARY_VAR_H_
