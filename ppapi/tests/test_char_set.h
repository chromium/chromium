// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CHAR_SET_H_
#define PPAPI_TESTS_TEST_CHAR_SET_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/trusted/ppb_char_set_trusted.h"
#include "ppapi/tests/test_case.h"

class TestCharSet : public TestCase {
 public:
  TestCharSet(TestingInstance* instance);

  // TestCase implementation.

  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestUTF16ToCharSetDeprecated();
  std::string TestUTF16ToCharSet();
  std::string TestCharSetToUTF16Deprecated();
  std::string TestCharSetToUTF16();
  std::string TestGetDefaultCharSet();

  // Converts the given UTF-8 string to a NON-NULL TERMINATED UTF-16 string
  // stored in the given vector.
  std::vector<uint16_t> UTF8ToUTF16(const std::string& utf8);

  const PPB_CharSet_Dev* char_set_interface_;
  const PPB_CharSet_Trusted* char_set_trusted_interface_;
};

#endif  // PPAPI_TESTS_TEST_CHAR_SET_H_
