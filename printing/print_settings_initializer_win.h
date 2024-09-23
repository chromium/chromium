// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_SETTINGS_INITIALIZER_WIN_H_
#define PRINTING_PRINT_SETTINGS_INITIALIZER_WIN_H_

#include "printing/page_range.h"
#include "printing/windows_types.h"

namespace printing {

class PrintSettings;

// Initializes a PrintSettings object from the provided device context.
class COMPONENT_EXPORT(PRINTING) PrintSettingsInitializerWin {
 public:
  PrintSettingsInitializerWin() = delete;
  PrintSettingsInitializerWin(const PrintSettingsInitializerWin&) = delete;
  PrintSettingsInitializerWin& operator=(const PrintSettingsInitializerWin&) =
      delete;

  static void InitPrintSettings(HDC hdc,
                                const DEVMODE& dev_mode,
                                PrintSettings* print_settings);
};

}  // namespace printing

#endif  // PRINTING_PRINT_SETTINGS_INITIALIZER_WIN_H_
