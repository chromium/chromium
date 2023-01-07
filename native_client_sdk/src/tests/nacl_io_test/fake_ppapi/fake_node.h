// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_NODE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_NODE_H_

#include <string>
#include <vector>

#include "ppapi/c/pp_file_info.h"

class FakeNode {
 public:
  FakeNode(const PP_FileInfo& info);
  FakeNode(const PP_FileInfo& info, const std::vector<uint8_t>& contents);
  FakeNode(const PP_FileInfo& info, const std::string& contents);

  int32_t Read(int64_t offset, char* buffer, int32_t bytes_to_read);
  int32_t Write(int64_t offset, const char* buffer, int32_t bytes_to_write);
  int32_t Append(const char* buffer, int32_t bytes_to_write);
  int32_t SetLength(int64_t length);
  void GetInfo(PP_FileInfo* out_info);
  bool IsRegular() const;
  bool IsDirectory() const;
  PP_FileType file_type() const { return info_.type; }

  // These times are not modified by the fake implementation.
  void set_creation_time(PP_Time time) { info_.creation_time = time; }
  void set_last_access_time(PP_Time time) { info_.last_access_time = time; }
  void set_last_modified_time(PP_Time time) { info_.last_modified_time = time; }

  const std::vector<uint8_t>& contents() const { return contents_; }

 private:
  PP_FileInfo info_;
  std::vector<uint8_t> contents_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_NODE_H_
