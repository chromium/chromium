// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer_mac.h"

#include <stdint.h>

#include "base/strings/sys_string_conversions.h"
#include "printing/print_settings.h"
#include "printing/units.h"

namespace printing {

// static
void PrintSettingsInitializerMac::InitPrintSettings(
    PMPrinter printer,
    PMPageFormat page_format,
    PrintSettings* print_settings) {
  print_settings->set_device_name(
      base::SysCFStringRefToUTF16(PMPrinterGetID(printer)));

  PMOrientation orientation = kPMPortrait;
  PMGetOrientation(page_format, &orientation);
  print_settings->SetOrientation(orientation == kPMLandscape);

  UInt32 resolution_count = 0;
  PMResolution best_resolution = {kDefaultMacDpi, kDefaultMacDpi};
  OSStatus status =
      PMPrinterGetPrinterResolutionCount(printer, &resolution_count);
  if (status == noErr) {
    // Resolution indexes are 1-based.
    for (uint32_t i = 1; i <= resolution_count; ++i) {
      PMResolution resolution;
      PMPrinterGetIndexedPrinterResolution(printer, i, &resolution);
      if (resolution.hRes > best_resolution.hRes)
        best_resolution = resolution;
    }
  }
  int dpi = best_resolution.hRes;
  print_settings->set_dpi(dpi);

  DCHECK_EQ(dpi, best_resolution.vRes);

  // Get printable area and paper rects (in points)
  PMRect page_rect, paper_rect;
  PMGetAdjustedPageRect(page_format, &page_rect);
  PMGetAdjustedPaperRect(page_format, &paper_rect);

  // Device units are in points. Units per inch is 72.
  gfx::Size physical_size_device_units((paper_rect.right - paper_rect.left),
                                       (paper_rect.bottom - paper_rect.top));
  gfx::Rect printable_area_device_units(
      (page_rect.left - paper_rect.left), (page_rect.top - paper_rect.top),
      (page_rect.right - page_rect.left), (page_rect.bottom - page_rect.top));

  DCHECK_EQ(print_settings->device_units_per_inch(), kPointsPerInch);
  print_settings->SetPrinterPrintableArea(physical_size_device_units,
                                          printable_area_device_units, false);
}

}  // namespace printing
