// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_UTIL_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_UTIL_H_

#include <string>

#include <ppapi/c/pp_completion_callback.h>

#include "fake_ppapi/fake_filesystem.h"
#include "fake_ppapi/fake_resource_manager.h"

const int32_t STATUSCODE_NOT_IMPLEMENTED = 501;

class FakeFileRefResource : public FakeResource {
 public:
  FakeFileRefResource() : filesystem(NULL) {}
  static const char* classname() { return "FakeFileRefResource"; }

  FakeFilesystem* filesystem;  // Weak reference.
  FakeFilesystem::Path path;
  std::string contents;
};

class FakeFileSystemResource : public FakeResource {
 public:
  FakeFileSystemResource() : filesystem(NULL), opened(false) {}
  ~FakeFileSystemResource() { delete filesystem; }
  static const char* classname() { return "FakeFileSystemResource"; }

  FakeFilesystem* filesystem;  // Owned.
  bool opened;
};

class FakeHtml5FsResource : public FakeResource {
 public:
  FakeHtml5FsResource() : filesystem_template(NULL) {}
  static const char* classname() { return "FakeHtml5FsResource"; }

  FakeFilesystem* filesystem_template;  // Weak reference.
};

class FakeURLRequestInfoResource : public FakeResource {
 public:
  FakeURLRequestInfoResource() : stream_to_file(false) {}
  static const char* classname() { return "FakeURLRequestInfoResource"; }

  std::string url;
  std::string method;
  std::string headers;
  std::string body;
  bool stream_to_file;
};

class FakeURLResponseInfoResource : public FakeResource {
 public:
  FakeURLResponseInfoResource() : status_code(0) {}
  static const char* classname() { return "FakeURLResponseInfoResource"; }

  int status_code;
  std::string url;
  std::string headers;
};

int32_t RunCompletionCallback(PP_CompletionCallback* callback, int32_t result);
bool GetHeaderValue(const std::string& headers,
                    const std::string& key,
                    std::string* out_value);
void SetHeader(const std::string& key,
               const std::string& value,
               std::string* out_headers);

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_UTIL_H_
