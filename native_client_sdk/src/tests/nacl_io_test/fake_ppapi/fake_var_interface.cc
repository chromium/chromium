// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_var_interface.h"
#include "fake_ppapi/fake_var_manager.h"
#include "gtest/gtest.h"

FakeVarInterface::FakeVarInterface(FakeVarManager* manager)
    : manager_(manager) {}

PP_Var FakeVarInterface::VarFromUtf8(const char* data, uint32_t len) {
  FakeVarData* var_data = manager_->CreateVarData();
  var_data->type = PP_VARTYPE_STRING;
  var_data->string_value.assign(data, len);

  struct PP_Var result = {PP_VARTYPE_STRING, 0, {PP_FALSE}};
  result.value.as_id = var_data->id;
  return result;
}

void FakeVarInterface::AddRef(PP_Var var) {
  manager_->AddRef(var);
}

void FakeVarInterface::Release(PP_Var var) {
  manager_->Release(var);
}

const char* FakeVarInterface::VarToUtf8(PP_Var var, uint32_t* out_len) {
  if (var.type != PP_VARTYPE_STRING) {
    *out_len = 0;
    return NULL;
  }

  FakeVarData* var_data = manager_->GetVarData(var);
  if (!var_data) {
    *out_len = 0;
    return NULL;
  }

  EXPECT_LT(0, var_data->ref_count) << "VarToUtf8 on freed "
                                    << manager_->Describe(*var_data);

  *out_len = var_data->string_value.length();
  return var_data->string_value.c_str();
}
