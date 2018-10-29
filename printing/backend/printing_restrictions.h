// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINTING_RESTRICTIONS_H_
#define PRINTING_BACKEND_PRINTING_RESTRICTIONS_H_

#include <vector>

#include "base/optional.h"
#include "printing/printing_export.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

// Allowed printing modes as a bitmask.
// This is used in pref file and should never change.
enum class ColorModeRestriction {
  kNone = 0x0,
  kMonochrome = 0x1,
  kColor = 0x2,
};

// Allowed duplex modes as a bitmask.
// This is used in pref file and should never change.
enum class DuplexModeRestriction {
  kNone = 0x0,
  kSimplex = 0x1,
  kLongEdge = 0x2,
  kShortEdge = 0x4,
  kDuplex = 0x2 | 0x4,
};

struct PRINTING_EXPORT PrintingRestrictions {
  PrintingRestrictions();
  ~PrintingRestrictions();

  // A bitmask of |ColorModeRestriction| specifying the allowed color modes.
  ColorModeRestriction color_modes;

  // A bitmask of |DuplexModeRestriction| specifying the allowed duplex modes.
  DuplexModeRestriction duplex_modes;

  // List of page sizes in microns.
  std::vector<gfx::Size> page_sizes_um;
};

// Dictionary key for printing policies.
// Must coincide with the name of field in |print_preview.Policies| in
// chrome/browser/resources/print_preview/native_layer.js
PRINTING_EXPORT extern const char kAllowedColorModes[];
PRINTING_EXPORT extern const char kAllowedDuplexModes[];

// Dictionary keys to be used with |kPrintingAllowedPageSizes| and
// |kPrintingSizeDefault| policies.
PRINTING_EXPORT extern const char kPageWidthUm[];
PRINTING_EXPORT extern const char kPageHeightUm[];

// Translate color mode from |kPrintingColorDefault| policy to
// |ColorModeRestriction| enum. Invalid values translated as |base::nullopt|.
base::Optional<ColorModeRestriction> PRINTING_EXPORT
GetColorModeForName(const std::string& mode_name);

// Translate color mode from |kPrintingAllowedColorModes| policy to
// |ColorModeRestriction| enum. Invalid values translated as |base::nullopt|.
base::Optional<ColorModeRestriction> PRINTING_EXPORT
GetAllowedColorModesForName(const std::string& mode_name);

// Translate duplex mode from |kPrintingDuplexDefault| policy to
// |DuplexModeRestriction| enum. Invalid values translated as |base::nullopt|.
base::Optional<DuplexModeRestriction> PRINTING_EXPORT
GetDuplexModeForName(const std::string& mode_name);

// Translate color mode from |kPrintingAllowedDuplexModes| policy to
// |DuplexModeRestriction| enum. Invalid values translated as |base::nullopt|.
base::Optional<DuplexModeRestriction> PRINTING_EXPORT
GetAllowedDuplexModesForName(const std::string& mode_name);

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINTING_RESTRICTIONS_H_
