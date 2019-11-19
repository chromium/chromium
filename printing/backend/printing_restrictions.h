// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINTING_RESTRICTIONS_H_
#define PRINTING_BACKEND_PRINTING_RESTRICTIONS_H_

#include "printing/printing_export.h"

namespace printing {

// Allowed printing modes as a bitmask.
// This is used in pref file and should never change.
enum class ColorModeRestriction {
  kUnset = 0x0,
  kMonochrome = 0x1,
  kColor = 0x2,
};

// Allowed duplex modes as a bitmask.
// This is used in pref file and should never change.
enum class DuplexModeRestriction {
  kUnset = 0x0,
  kSimplex = 0x1,
  kLongEdge = 0x2,
  kShortEdge = 0x4,
  kDuplex = 0x2 | 0x4,
};

// Allowed PIN printing modes.
// This is used in pref file and should never change.
enum class PinModeRestriction {
  kUnset = 0,
  kPin = 1,
  kNoPin = 2,
};

// Allowed background graphics modes.
// This is used in pref file and should never change.
enum class BackgroundGraphicsModeRestriction {
  kUnset = 0,
  kEnabled = 1,
  kDisabled = 2,
};

// Dictionary key for printing policies.
// Must coincide with the name of field in |print_preview.Policies| in
// chrome/browser/resources/print_preview/data/destination.js
PRINTING_EXPORT extern const char kAllowedColorModes[];
PRINTING_EXPORT extern const char kAllowedDuplexModes[];
PRINTING_EXPORT extern const char kAllowedPinModes[];
PRINTING_EXPORT extern const char kAllowedBackgroundGraphicsModes[];
PRINTING_EXPORT extern const char kDefaultColorMode[];
PRINTING_EXPORT extern const char kDefaultDuplexMode[];
PRINTING_EXPORT extern const char kDefaultPinMode[];
PRINTING_EXPORT extern const char kDefaultBackgroundGraphicsMode[];

// Dictionary keys to be used with |kPrintingAllowedPageSizes| and
// |kPrintingSizeDefault| policies.
PRINTING_EXPORT extern const char kPageWidthUm[];
PRINTING_EXPORT extern const char kPageHeightUm[];

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINTING_RESTRICTIONS_H_
