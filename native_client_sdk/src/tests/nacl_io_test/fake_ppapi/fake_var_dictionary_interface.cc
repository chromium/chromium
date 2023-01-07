// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_var_dictionary_interface.h"

#include "fake_ppapi/fake_var_manager.h"
#include "gtest/gtest.h"

FakeVarDictionaryInterface::FakeVarDictionaryInterface(FakeVarManager* manager,
    nacl_io::VarInterface* var_interface,
    nacl_io::VarArrayInterface* array_interface) :
  manager_(manager),
  var_interface_(var_interface),
  array_interface_(array_interface) {}


PP_Var FakeVarDictionaryInterface::Create() {
  FakeVarData* var_data = manager_->CreateVarData();
  var_data->type = PP_VARTYPE_DICTIONARY;

  struct PP_Var result = {PP_VARTYPE_DICTIONARY, 0, {PP_FALSE}};
  result.value.as_id = var_data->id;
  return result;
}

PP_Bool FakeVarDictionaryInterface::Set(PP_Var var, PP_Var key, PP_Var value) {
  EXPECT_EQ(PP_VARTYPE_DICTIONARY, var.type);
  EXPECT_EQ(PP_VARTYPE_STRING, key.type);
  FakeVarData* data = manager_->GetVarData(var);
  FakeVarData* key_data = manager_->GetVarData(key);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), key_data);
  const std::string& key_string = key_data->string_value;
  FakeDictType& dict = data->dict_value;
  manager_->AddRef(value);
  // Release any existing value
  if (dict.count(key_string) > 0) {
    manager_->Release(dict[key_string]);
  }
  dict[key_string] = value;
  return PP_TRUE;
}

PP_Var FakeVarDictionaryInterface::Get(PP_Var var, PP_Var key) {
  EXPECT_EQ(PP_VARTYPE_DICTIONARY, var.type);
  EXPECT_EQ(PP_VARTYPE_STRING, key.type);
  FakeVarData* data = manager_->GetVarData(var);
  FakeVarData* key_data = manager_->GetVarData(key);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), key_data);
  FakeDictType& dict = data->dict_value;
  const std::string& key_string = key_data->string_value;
  PP_Var rtn = dict[key_string];
  manager_->AddRef(rtn);
  return rtn;
}

void FakeVarDictionaryInterface::Delete(PP_Var var, PP_Var key) {
  EXPECT_EQ(PP_VARTYPE_DICTIONARY, var.type);
  EXPECT_EQ(PP_VARTYPE_STRING, key.type);
  FakeVarData* data = manager_->GetVarData(var);
  FakeVarData* key_data = manager_->GetVarData(key);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), key_data);
  FakeDictType& dict = data->dict_value;
  const std::string& key_string = key_data->string_value;
  if (dict.count(key_string) > 0) {
    manager_->Release(dict[key_string]);
  }
  dict.erase(key_string);
}

PP_Bool FakeVarDictionaryInterface::HasKey(PP_Var var, PP_Var key) {
  EXPECT_EQ(PP_VARTYPE_DICTIONARY, var.type);
  EXPECT_EQ(PP_VARTYPE_STRING, key.type);
  FakeVarData* data = manager_->GetVarData(var);
  FakeVarData* key_data = manager_->GetVarData(key);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), key_data);
  FakeDictType& dict = data->dict_value;
  const std::string& key_string = key_data->string_value;
  if (dict.count(key_string) > 0)
    return PP_FALSE;
  return PP_TRUE;
}

PP_Var FakeVarDictionaryInterface::GetKeys(PP_Var var) {
  EXPECT_EQ(PP_VARTYPE_DICTIONARY, var.type);
  FakeVarData* data = manager_->GetVarData(var);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  FakeDictType& dict = data->dict_value;
  PP_Var rtn = array_interface_->Create();
  array_interface_->SetLength(rtn, dict.size());
  int index = 0;
  for (FakeDictType::iterator it = dict.begin(); it != dict.end(); it++) {
    PP_Var key = var_interface_->VarFromUtf8(it->first.c_str(),
                                             it->first.size());
    array_interface_->Set(rtn, index, key);
    manager_->Release(key);
    index++;
  }
  return rtn;
}
