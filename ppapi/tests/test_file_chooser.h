// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_FILE_CHOOSER_H_
#define PPAPI_TESTS_TEST_FILE_CHOOSER_H_

#include "ppapi/tests/test_case.h"

namespace pp {
class FileRef;
}

class TestFileChooser : public TestCase {
 public:
  TestFileChooser(TestingInstance* instance) : TestCase(instance) {}

  // TestCase
  bool Init() override;
  void RunTests(const std::string& filter) override;

 private:
  // Writes the string "Hello from PPAPI" into the file represented by
  // |file_ref|. Returns true on success.
  bool WriteDefaultContentsToFile(const pp::FileRef& file_ref);

  std::string TestOpenSimple();
  std::string TestOpenCancel();
  std::string TestSaveAsSafeDefaultName();
  std::string TestSaveAsUnsafeDefaultName();
  std::string TestSaveAsCancel();
  std::string TestSaveAsDangerousExecutableAllowed();
  std::string TestSaveAsDangerousExecutableDisallowed();
  std::string TestSaveAsDangerousExtensionListDisallowed();
};

#endif  // PPAPI_TESTS_TEST_FILE_CHOOSER_H_
