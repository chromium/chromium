// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_BASE_TEST_UNITTEST_H_
#define COURGETTE_BASE_TEST_UNITTEST_H_

#include <list>
#include <string>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

class BaseTest : public testing::Test {
 public:
  std::string FileContents(const char* file_name) const;

  // Pass a list of strings, and get back the concatenated contents
  // of each of the mentioned files.
  std::string FilesContents(std::list<std::string> file_names) const;

 private:
  virtual void SetUp();
  virtual void TearDown();

  base::FilePath test_dir_;
};

#endif  // COURGETTE_BASE_TEST_UNITTEST_H_
