// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_TEST_H_
#define PRINTING_PRINTING_TEST_H_

#include <windows.h>
#include <winspool.h>

#include <string>

#include "base/stl_util.h"

// Disable the whole test case when executing on a computer that has no printer
// installed.
// Note: Parent should be testing::Test or InProcessBrowserTest.
template <typename Parent>
class PrintingTest : public Parent {
 public:
  static bool IsTestCaseDisabled() { return GetDefaultPrinter().empty(); }
  static std::wstring GetDefaultPrinter() {
    wchar_t printer_name[MAX_PATH];
    DWORD size = base::size(printer_name);
    BOOL result = ::GetDefaultPrinter(printer_name, &size);
    if (result == 0) {
      if (GetLastError() == ERROR_FILE_NOT_FOUND) {
        printf("There is no printer installed, printing can't be tested!\n");
        return std::wstring();
      }
      printf("INTERNAL PRINTER ERROR!\n");
      return std::wstring();
    }
    return printer_name;
  }
};

#endif  // PRINTING_PRINTING_TEST_H_
