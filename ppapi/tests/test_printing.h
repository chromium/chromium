// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_PRINTING_H_
#define PPAPI_TESTS_TEST_PRINTING_H_

#include <stdint.h>

#include <string>

#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/utility/completion_callback_factory.h"

struct PP_PrintSettings_Dev;

class TestPrinting : public TestCase {
 public:
  explicit TestPrinting(TestingInstance* instance);

  // TestCase implementation.
  virtual void RunTests(const std::string& filter);

 private:
  // Tests.
  std::string TestGetDefaultPrintSettings();

  void Callback(int32_t result,
                PP_PrintSettings_Dev&);

  NestedEvent nested_event_;

  pp::CompletionCallbackFactory<TestPrinting> callback_factory_;
};

#endif  // PPAPI_TESTS_TEST_PRINTING_H_
