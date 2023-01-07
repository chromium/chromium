// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILE_SYSTEM_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILE_SYSTEM_INTERFACE_H_

#include "fake_ppapi/fake_core_interface.h"
#include "sdk_util/macros.h"

class FakeFileSystemInterface : public nacl_io::FileSystemInterface {
 public:
  FakeFileSystemInterface(FakeCoreInterface* core_interface);

  FakeFileSystemInterface(const FakeFileSystemInterface&) = delete;
  FakeFileSystemInterface& operator=(const FakeFileSystemInterface&) = delete;

  virtual PP_Bool IsFileSystem(PP_Resource resource);
  virtual PP_Resource Create(PP_Instance instance, PP_FileSystemType type);
  virtual int32_t Open(PP_Resource file_system,
                       int64_t expected_size,
                       PP_CompletionCallback callback);

 private:
  FakeCoreInterface* core_interface_;  // Weak reference.
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILE_SYSTEM_INTERFACE_H_
