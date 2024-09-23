// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_TEST_H_
#define PRINTING_PRINTING_TEST_H_

#include <windows.h>

#include <winspool.h>

#include <string>

#include "printing/backend/spooler_win.h"

// Disable the whole test case when executing on a computer that has no printer
// installed.
// Note: Parent should be testing::Test or InProcessBrowserTest.
template <typename Parent>
class PrintingTest : public Parent {
 public:
  static bool IsTestCaseDisabled() { return GetDefaultPrinter().empty(); }
  static std::wstring GetDefaultPrinter() {
    wchar_t printer_name[MAX_PATH];
    DWORD size = std::size(printer_name);
    BOOL result = ::GetDefaultPrinter(printer_name, &size);
    if (result == 0) {
      if (printing::internal::IsSpoolerRunning() !=
          printing::internal::SpoolerServiceStatus::kRunning) {
        printf("The Windows print spooler service is not running!\n");
        return std::wstring();
      }
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
