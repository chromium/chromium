// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_DICTIONARY_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_DICTIONARY_INTERFACE_H_

#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

class FakeVarManager;

class FakeVarDictionaryInterface : public nacl_io::VarDictionaryInterface {
 public:
  explicit FakeVarDictionaryInterface(FakeVarManager* manager,
      nacl_io::VarInterface* var_interface,
      nacl_io::VarArrayInterface* array_interface);

  FakeVarDictionaryInterface(const FakeVarDictionaryInterface&) = delete;
  FakeVarDictionaryInterface& operator=(const FakeVarDictionaryInterface&) =
      delete;

  virtual PP_Var Create();
  virtual PP_Var Get(PP_Var dict, PP_Var key);
  virtual PP_Bool Set(PP_Var dict, PP_Var key, PP_Var value);
  virtual void  Delete(PP_Var dict, PP_Var key);
  virtual PP_Bool HasKey(PP_Var dict, PP_Var key);
  virtual PP_Var GetKeys(PP_Var dict);

 private:
  FakeVarManager* manager_;
  nacl_io::VarInterface* var_interface_;
  nacl_io::VarArrayInterface* array_interface_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_DICTIONARY_INTERFACE_H_
