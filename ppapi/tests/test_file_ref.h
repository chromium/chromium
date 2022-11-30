// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_FILE_REF_H_
#define PPAPI_TESTS_TEST_FILE_REF_H_

#include <stdint.h>

#include <string>

#include "ppapi/tests/test_case.h"

namespace pp {
class FileRef;
}

class TestFileRef : public TestCase {
 public:
  explicit TestFileRef(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  // Creates a FileRef on an external filesystem.
  // Returns "" on success, a different string otherwise.
  std::string MakeExternalFileRef(pp::FileRef* file_ref_ext);

  int32_t DeleteDirectoryRecursively(pp::FileRef* dir);

  std::string TestCreate();
  std::string TestGetFileSystemType();
  std::string TestGetName();
  std::string TestGetPath();
  std::string TestGetParent();
  std::string TestMakeDirectory();
  std::string TestQueryAndTouchFile();
  std::string TestDeleteFileAndDirectory();
  std::string TestRenameFileAndDirectory();
  std::string TestQuery();
  std::string TestFileNameEscaping();
  std::string TestReadDirectoryEntries();
};

#endif  // PPAPI_TESTS_TEST_FILE_REF_H_
