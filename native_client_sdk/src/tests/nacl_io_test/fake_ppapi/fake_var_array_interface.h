// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_ARRAY_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_ARRAY_INTERFACE_H_

#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

class FakeVarManager;

class FakeVarArrayInterface : public nacl_io::VarArrayInterface {
 public:
  explicit FakeVarArrayInterface(FakeVarManager* manager);

  FakeVarArrayInterface(const FakeVarArrayInterface&) = delete;
  FakeVarArrayInterface& operator=(const FakeVarArrayInterface&) = delete;

  virtual PP_Var Create();
  virtual PP_Var Get(PP_Var array, uint32_t index);
  virtual PP_Bool Set(PP_Var array, uint32_t index, PP_Var value);
  virtual uint32_t GetLength(PP_Var array);
  virtual PP_Bool SetLength(PP_Var array, uint32_t length);

 private:
  FakeVarManager* manager_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_ARRAY_INTERFACE_H_
