// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILE_REF_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILE_REF_INTERFACE_H_

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_var_interface.h"
#include "fake_ppapi/fake_var_manager.h"
#include "sdk_util/macros.h"

class FakeFileRefInterface : public nacl_io::FileRefInterface {
 public:
  FakeFileRefInterface(FakeCoreInterface* core_interface,
                       FakeVarInterface* var_interface);

  FakeFileRefInterface(const FakeFileRefInterface&) = delete;
  FakeFileRefInterface& operator=(const FakeFileRefInterface&) = delete;

  virtual PP_Resource Create(PP_Resource file_system, const char* path);
  virtual PP_Var GetName(PP_Resource file_ref);
  virtual int32_t MakeDirectory(PP_Resource directory_ref,
                                PP_Bool make_parents,
                                PP_CompletionCallback callback);
  virtual int32_t Delete(PP_Resource file_ref, PP_CompletionCallback callback);
  virtual int32_t Query(PP_Resource file_ref,
                        PP_FileInfo* info,
                        PP_CompletionCallback callback);
  virtual int32_t ReadDirectoryEntries(PP_Resource file_ref,
                                       const PP_ArrayOutput& output,
                                       PP_CompletionCallback callback);
  virtual int32_t Rename(PP_Resource file_ref,
                         PP_Resource new_file_ref,
                         PP_CompletionCallback callback);

 private:
  FakeCoreInterface* core_interface_;  // Weak reference.
  FakeVarInterface* var_interface_;    // Weak reference.
  FakeVarManager* var_manager_;        // Weak reference
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILE_REF_INTERFACE_H_
