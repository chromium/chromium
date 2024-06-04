// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_
#define PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(USE_CUPS)
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend.h"
#endif  // BUILDFLAG(USE_CUPS)

namespace gfx {
class Size;
}

namespace printing {

enum class Unit {
  kInches,
  kMillimeters,
};

// Parses the media name expressed by `value` into the size of the media
// in microns. Returns an empty size if `value` does not contain the display
// name nor the dimension, or if `value` contains a prefix of
// media sizes not meant for users' eyes.
COMPONENT_EXPORT(PRINT_BACKEND)
gfx::Size ParsePaperSize(std::string_view value);

#if BUILDFLAG(USE_CUPS)
// Helper functions to resolve display name, this is used on
// multiple places to encapsulate differences in CUPS implementations on
// various platforms.
COMPONENT_EXPORT(PRINT_BACKEND)
std::string GetDisplayName(const std::string& printer_name,
                           std::string_view info);

// Helper functions to resolve printer description, this is used on
// multiple places to encapsulate differences in CUPS implementations on
// various platforms.
COMPONENT_EXPORT(PRINT_BACKEND)
std::string_view GetPrinterDescription(std::string_view drv_info,
                                       std::string_view info);

// Calculates a paper's printable area in microns from its size in microns and
// its four margins in PWG units.
COMPONENT_EXPORT(PRINT_BACKEND)
gfx::Rect PrintableAreaFromSizeAndPwgMargins(const gfx::Size& size_um,
                                             int bottom_pwg,
                                             int left_pwg,
                                             int right_pwg,
                                             int top_pwg);

// Calculates a paper's four margins in PWG units from its size and printable
// area in microns. Since the size and printable area were converted from PWG
// units in the first place, the margins in PWG units can be reconstructed
// losslessly.
COMPONENT_EXPORT(PRINT_BACKEND)
void PwgMarginsFromSizeAndPrintableArea(const gfx::Size& size_um,
                                        const gfx::Rect& printable_area_um,
                                        int* bottom_pwg,
                                        int* left_pwg,
                                        int* right_pwg,
                                        int* top_pwg);
#endif  // BUILDFLAG(USE_CUPS)

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_
