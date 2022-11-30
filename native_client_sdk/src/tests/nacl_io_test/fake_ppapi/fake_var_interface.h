// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_INTERFACE_H_

#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

class FakeVarManager;

class FakeVarInterface : public nacl_io::VarInterface {
 public:
  explicit FakeVarInterface(FakeVarManager* manager);

  FakeVarInterface(const FakeVarInterface&) = delete;
  FakeVarInterface& operator=(const FakeVarInterface&) = delete;

  virtual void AddRef(PP_Var var);
  virtual void Release(PP_Var var);
  virtual PP_Var VarFromUtf8(const char* data, uint32_t len);
  virtual const char* VarToUtf8(PP_Var var, uint32_t* out_len);

 private:
  FakeVarManager* manager_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_INTERFACE_H_
