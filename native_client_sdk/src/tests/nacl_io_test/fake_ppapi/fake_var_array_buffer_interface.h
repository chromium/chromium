// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_ARRAY_BUFFER_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_ARRAY_BUFFER_INTERFACE_H_

#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

class FakeVarManager;

class FakeVarArrayBufferInterface : public nacl_io::VarArrayBufferInterface {
 public:
  explicit FakeVarArrayBufferInterface(FakeVarManager* manager);

  FakeVarArrayBufferInterface(const FakeVarArrayBufferInterface&) = delete;
  FakeVarArrayBufferInterface& operator=(const FakeVarArrayBufferInterface&) =
      delete;

  virtual struct PP_Var Create(uint32_t size_in_bytes);
  virtual PP_Bool ByteLength(struct PP_Var var, uint32_t* byte_length);
  virtual void* Map(struct PP_Var var);
  virtual void Unmap(struct PP_Var var);

 private:
  FakeVarManager* manager_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_ARRAY_BUFFER_INTERFACE_H_
