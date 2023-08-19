// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_UNITS_H_
#define PRINTING_UNITS_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace printing {

// Length of an inch in 0.001mm unit.
constexpr int kMicronsPerInch = 25400;

// A micron is a thousandth of a mm.
constexpr int kMicronsPerMm = 1000;

// Length of a PWG unit in 0.001mm unit.
constexpr int kMicronsPerPwgUnit = kMicronsPerMm / 100;

// Mil is a thousandth of an inch.
constexpr float kMicronsPerMil = 25.4f;
constexpr int kMilsPerInch = 1000;

// Length of an inch in CSS's 1pt unit.
// http://dev.w3.org/csswg/css3-values/#absolute-length-units-cm-mm.-in-pt-pc
constexpr int kPointsPerInch = 72;

// Length of an inch in CSS's 1px unit.
// http://dev.w3.org/csswg/css3-values/#the-px-unit
constexpr int kPixelsPerInch = 96;

#if BUILDFLAG(IS_MAC)
constexpr int kDefaultMacDpi = 72;
#endif  // BUILDFLAG(IS_MAC)

// DPI used for Save to PDF. Also used as the default DPI in various use cases
// where there is no specified DPI.
constexpr int kDefaultPdfDpi = 300;

// LETTER: 8.5 x 11 inches
constexpr float kLetterWidthInch = 8.5f;
constexpr float kLetterHeightInch = 11.0f;

// LEGAL: 8.5 x 14 inches
constexpr float kLegalWidthInch = 8.5f;
constexpr float kLegalHeightInch = 14.0f;

// A4: 8.27 x 11.69 inches
constexpr float kA4WidthInch = 8.27f;
constexpr float kA4HeightInch = 11.69f;

// A3: 11.69 x 16.54 inches
constexpr float kA3WidthInch = 11.69f;
constexpr float kA3HeightInch = 16.54f;

// Converts from one unit system to another using integer arithmetics.
COMPONENT_EXPORT(PRINTING_BASE)
int ConvertUnit(float value, int old_unit, int new_unit);

// Converts from one unit system to another using floats.
COMPONENT_EXPORT(PRINTING_BASE)
float ConvertUnitFloat(float value, float old_unit, float new_unit);

}  // namespace printing

#endif  // PRINTING_UNITS_H_
