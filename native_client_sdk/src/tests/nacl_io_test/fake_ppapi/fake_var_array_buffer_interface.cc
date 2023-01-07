// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_var_array_buffer_interface.h"

#include "fake_ppapi/fake_var_manager.h"
#include "gtest/gtest.h"

FakeVarArrayBufferInterface::FakeVarArrayBufferInterface(
    FakeVarManager* manager) : manager_(manager) {}

struct PP_Var FakeVarArrayBufferInterface::Create(uint32_t size_in_bytes) {
  FakeVarData* var_data = manager_->CreateVarData();
  var_data->type = PP_VARTYPE_ARRAY_BUFFER;
  var_data->buffer_value.length = size_in_bytes;
  var_data->buffer_value.ptr = malloc(size_in_bytes);

  struct PP_Var result = {PP_VARTYPE_ARRAY_BUFFER, 0, {PP_FALSE}};
  result.value.as_id = var_data->id;
  return result;
}

PP_Bool FakeVarArrayBufferInterface::ByteLength(struct PP_Var var,
                                                uint32_t* byte_length) {
  EXPECT_EQ(PP_VARTYPE_ARRAY_BUFFER, var.type);
  FakeVarData* data = manager_->GetVarData(var);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  *byte_length = data->buffer_value.length;
  return PP_TRUE;
}

void* FakeVarArrayBufferInterface::Map(struct PP_Var var) {
  EXPECT_EQ(PP_VARTYPE_ARRAY_BUFFER, var.type);
  FakeVarData* data = manager_->GetVarData(var);
  EXPECT_NE(static_cast<FakeVarData*>(NULL), data);
  return data->buffer_value.ptr;
}

void FakeVarArrayBufferInterface::Unmap(struct PP_Var var) {
  ASSERT_EQ(PP_VARTYPE_ARRAY_BUFFER, var.type);
}
