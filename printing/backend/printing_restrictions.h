// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINTING_RESTRICTIONS_H_
#define PRINTING_BACKEND_PRINTING_RESTRICTIONS_H_

#include "base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "printing/mojom/print.mojom.h"
#endif

namespace printing {

#if BUILDFLAG(IS_CHROMEOS)
// Allowed printing modes as a bitmask.
// This is used in pref file and crosapi. It should never change.
using ColorModeRestriction = mojom::ColorModeRestriction;

// Allowed duplex modes as a bitmask.
// This is used in pref file and crosapi. It should never change.
using DuplexModeRestriction = mojom::DuplexModeRestriction;

// Allowed PIN printing modes.
// This is used in pref file and should never change.
using PinModeRestriction = mojom::PinModeRestriction;

// Dictionary key for printing policies.
// Must coincide with the name of field in `print_preview.Policies` in
// chrome/browser/resources/print_preview/data/destination.ts
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kAllowedColorModes[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kAllowedDuplexModes[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kAllowedPinModes[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kDefaultColorMode[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kDefaultDuplexMode[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kDefaultPinMode[];
#endif  // BUILDFLAG(IS_CHROMEOS)

// Allowed background graphics modes.
// This is used in pref file and should never change.
enum class BackgroundGraphicsModeRestriction {
  kUnset = 0,
  kEnabled = 1,
  kDisabled = 2,
};

// Dictionary keys to be used with `kPrintingPaperSizeDefault` policy.
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kPaperSizeName[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kPaperSizeNameCustomOption[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kPaperSizeCustomSize[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kPaperSizeWidth[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kPaperSizeHeight[];

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINTING_RESTRICTIONS_H_
