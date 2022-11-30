// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILE_IO_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILE_IO_INTERFACE_H_

#include "fake_ppapi/fake_core_interface.h"
#include "sdk_util/macros.h"

class FakeFileIoInterface : public nacl_io::FileIoInterface {
 public:
  explicit FakeFileIoInterface(FakeCoreInterface* core_interface);

  FakeFileIoInterface(const FakeFileIoInterface&) = delete;
  FakeFileIoInterface& operator=(const FakeFileIoInterface&) = delete;

  virtual PP_Resource Create(PP_Resource instance);
  virtual int32_t Open(PP_Resource file_io,
                       PP_Resource file_ref,
                       int32_t open_flags,
                       PP_CompletionCallback callback);
  virtual int32_t Query(PP_Resource file_io,
                        PP_FileInfo* info,
                        PP_CompletionCallback callback);
  virtual int32_t Read(PP_Resource file_io,
                       int64_t offset,
                       char* buffer,
                       int32_t bytes_to_read,
                       PP_CompletionCallback callback);
  virtual int32_t Write(PP_Resource file_io,
                        int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write,
                        PP_CompletionCallback callback);
  virtual int32_t SetLength(PP_Resource file_io,
                            int64_t length,
                            PP_CompletionCallback callback);
  virtual int32_t Flush(PP_Resource file_io, PP_CompletionCallback callback);
  virtual void Close(PP_Resource file_io);

 private:
  FakeCoreInterface* core_interface_;  // Weak reference.
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILE_IO_INTERFACE_H_
