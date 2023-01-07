// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_var_array_interface.h"

#include "fake_ppapi/fake_var_manager.h"
#include "gtest/gtest.h"

FakeVarArrayInterface::FakeVarArrayInterface(FakeVarManager* manager)
    : manager_(manager) {}


PP_Var FakeVarArrayInterface::Create() {
  FakeVarData* var_data = manager_->CreateVarData();
  var_data->type = PP_VARTYPE_ARRAY;

  struct PP_Var result = {PP_VARTYPE_ARRAY, 0, {PP_FALSE}};
  result.value.as_id = var_data->id;
  return result;
}

PP_Var FakeVarArrayInterface::Get(PP_Var var, uint32_t index) {
  EXPECT_EQ(var.type, PP_VARTYPE_ARRAY);
  FakeVarData* data = manager_->GetVarData(var);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  if (index >= data->array_value.size())
    return PP_MakeUndefined();

  // Return the item at the given index, after first incrementing
  // its refcount.  It is up the callee to then call Release.
  PP_Var result = data->array_value[index];
  manager_->AddRef(result);
  return result;
}

PP_Bool FakeVarArrayInterface::Set(PP_Var var, uint32_t index, PP_Var value) {
  EXPECT_EQ(var.type, PP_VARTYPE_ARRAY);
  FakeVarData* data = manager_->GetVarData(var);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  if (index >= data->array_value.size())
    data->array_value.resize(index+1);
  else
    manager_->Release(data->array_value[index]);
  data->array_value[index] = value;
  manager_->AddRef(value);
  return PP_TRUE;
}

uint32_t FakeVarArrayInterface::GetLength(PP_Var var) {
  EXPECT_EQ(var.type, PP_VARTYPE_ARRAY);
  FakeVarData* data = manager_->GetVarData(var);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  return data->array_value.size();
}

PP_Bool FakeVarArrayInterface::SetLength(PP_Var var, uint32_t length) {
  EXPECT_EQ(var.type, PP_VARTYPE_ARRAY);
  FakeVarData* data = manager_->GetVarData(var);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  data->array_value.resize(length);
  return PP_TRUE;
}
