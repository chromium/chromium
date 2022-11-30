// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_BROWSER_FONT_H_
#define PPAPI_TESTS_TEST_BROWSER_FONT_H_

#include "ppapi/tests/test_case.h"

class TestBrowserFont : public TestCase {
 public:
  TestBrowserFont(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestFontFamilies();
  std::string TestMeasure();
  std::string TestMeasureRTL();
  std::string TestCharPos();
  std::string TestCharPosRTL();
  std::string TestDraw();
};

#endif  // PPAPI_TESTS_TEST_BROWSER_FONT_H_
