// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_job_constants_cups.h"

namespace printing {

// Variations of identifier used for specifying printer color model.
// New ways of specifying a color model should include an entry in
// `kKnownPpdColorSettings`.
const char kCUPSColorMode[] = "ColorMode";
const char kCUPSColorModel[] = "ColorModel";
const char kCUPSPrintoutMode[] = "PrintoutMode";
const char kCUPSProcessColorModel[] = "ProcessColorModel";
const char kCUPSBrotherMonoColor[] = "BRMonoColor";
const char kCUPSBrotherPrintQuality[] = "BRPrintQuality";
const char kCUPSCanonCNColorMode[] = "CNColorMode";
const char kCUPSCanonCNIJGrayScale[] = "CNIJGrayScale";
const char kCUPSEpsonInk[] = "Ink";
const char kCUPSHpColorMode[] = "HPColorMode";
const char kCUPSHpPjlColorAsGray[] = "HPPJLColorAsGray";
const char kCUPSKonicaMinoltaSelectColor[] = "SelectColor";
const char kCUPSLexmarkBLW[] = "BLW";
const char kCUPSOkiControl[] = "OKControl";
const char kCUPSSharpARCMode[] = "ARCMode";
const char kCUPSXeroxXROutputColor[] = "XROutputColor";
const char kCUPSXeroxXRXColor[] = "XRXColor";

// Variations of identifier used for specifying printer color model choice.
const char kAuto[] = "Auto";
const char kBlack[] = "Black";
const char kCMYK[] = "CMYK";
const char kKCMY[] = "KCMY";
const char kCMY_K[] = "CMY+K";
const char kCMY[] = "CMY";
const char kColor[] = "Color";
const char kDraftGray[] = "Draft.Gray";
const char kEpsonColor[] = "COLOR";
const char kEpsonMono[] = "MONO";
const char kFullColor[] = "FullColor";
const char kGray[] = "Gray";
const char kGrayscale[] = "Grayscale";
const char kGreyscale[] = "Greyscale";
const char kHighGray[] = "High.Gray";
const char kHpColorPrint[] = "ColorPrint";
const char kHpGrayscalePrint[] = "GrayscalePrint";
const char kHpPjlColorAsGrayNo[] = "no";
const char kHpPjlColorAsGrayYes[] = "yes";
const char kLexmarkBLWFalse[] = "FalseM";
const char kLexmarkBLWTrue[] = "TrueM";
const char kMono[] = "Mono";
const char kMonochrome[] = "Monochrome";
const char kNormal[] = "Normal";
const char kNormalGray[] = "Normal.Gray";
const char kOne[] = "1";
const char kPrintAsColor[] = "PrintAsColor";
const char kPrintAsGrayscale[] = "PrintAsGrayscale";
const char kRGB[] = "RGB";
const char kRGBA[] = "RGBA";
const char kRGB16[] = "RGB16";
const char kSamsungColorFalse[] = "False";
const char kSamsungColorTrue[] = "True";
const char kSharpCMColor[] = "CMColor";
const char kSharpCMBW[] = "CMBW";
const char kXeroxAutomatic[] = "Automatic";
const char kXeroxBW[] = "BW";
const char kZero[] = "0";

#if BUILDFLAG(IS_MAC)
base::span<const PpdColorSetting> GetKnownPpdColorSettings() {
  static const PpdColorSetting kKnownPpdColorSettings[] = {
      {kCUPSBrotherMonoColor, kMono, kFullColor},            // Brother
      {kCUPSBrotherPrintQuality, kBlack, kColor},            // Brother
      {kCUPSCanonCNColorMode, kMono, kColor},                // Canon
      {kCUPSCanonCNIJGrayScale, kOne, kZero},                // Canon
      {kCUPSColorMode, kMonochrome, kColor},                 // Samsung
      {kCUPSColorModel, kGray, kColor},                      // Generic
      {kCUPSEpsonInk, kEpsonMono, kEpsonColor},              // Epson
      {kCUPSHpColorMode, kHpGrayscalePrint, kHpColorPrint},  // HP
      {kCUPSHpPjlColorAsGray, kHpPjlColorAsGrayYes, kHpPjlColorAsGrayNo},  // HP
      {kCUPSKonicaMinoltaSelectColor, kGrayscale, kColor},   // Konica Minolta
      {kCUPSLexmarkBLW, kLexmarkBLWTrue, kLexmarkBLWFalse},  // Lexmark
      {kCUPSOkiControl, kGray, kAuto},                       // Oki
      {kCUPSPrintoutMode, kNormalGray, kNormal},             // Foomatic
      {kCUPSSharpARCMode, kSharpCMBW, kSharpCMColor},        // Sharp
      {kCUPSXeroxXROutputColor, kPrintAsGrayscale, kPrintAsColor},  // Xerox
      {kCUPSXeroxXRXColor, kXeroxBW, kXeroxAutomatic},              // Xerox
  };
  return base::make_span(kKnownPpdColorSettings);
}
#endif

}  // namespace printing
