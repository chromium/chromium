// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_SETTINGS_INITIALIZER_MAC_H_
#define PRINTING_PRINT_SETTINGS_INITIALIZER_MAC_H_

#import <ApplicationServices/ApplicationServices.h>

#include "printing/page_range.h"

namespace printing {

class PrintSettings;

// Initializes a PrintSettings object from the provided device context.
class COMPONENT_EXPORT(PRINTING) PrintSettingsInitializerMac {
 public:
  PrintSettingsInitializerMac() = delete;
  PrintSettingsInitializerMac(const PrintSettingsInitializerMac&) = delete;
  PrintSettingsInitializerMac& operator=(const PrintSettingsInitializerMac&) =
      delete;

  static void InitPrintSettings(PMPrinter printer,
                                PMPageFormat page_format,
                                PrintSettings* print_settings);
};

}  // namespace printing

#endif  // PRINTING_PRINT_SETTINGS_INITIALIZER_MAC_H_
