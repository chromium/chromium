// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_PDF_H_
#define PPAPI_TESTS_TEST_PDF_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"

class TestPDF : public TestCase {
 public:
  explicit TestPDF(TestingInstance* instance);

  // TestCase implementation.
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestGetLocalizedString();
  std::string TestGetV8ExternalSnapshotData();
};

#endif  // PPAPI_TESTS_TEST_PDF_H_
