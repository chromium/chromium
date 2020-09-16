// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_JOB_CONSTANTS_H_
#define PRINTING_PRINT_JOB_CONSTANTS_H_

#include <stdint.h>

#include "printing/printing_export.h"

namespace printing {

PRINTING_EXPORT extern const char kIsFirstRequest[];
PRINTING_EXPORT extern const char kPreviewRequestID[];
PRINTING_EXPORT extern const char kPreviewUIID[];
PRINTING_EXPORT extern const char kSettingCapabilities[];
PRINTING_EXPORT extern const char kSettingCloudPrintId[];
PRINTING_EXPORT extern const char kSettingCollate[];
PRINTING_EXPORT extern const char kSettingColor[];
PRINTING_EXPORT extern const char kSettingSetColorAsDefault[];
PRINTING_EXPORT extern const char kSettingContentHeight[];
PRINTING_EXPORT extern const char kSettingContentWidth[];
PRINTING_EXPORT extern const char kSettingCopies[];
PRINTING_EXPORT extern const char kSettingDeviceName[];
PRINTING_EXPORT extern const char kSettingDisableScaling[];
PRINTING_EXPORT extern const char kSettingDpiDefault[];
PRINTING_EXPORT extern const char kSettingDpiHorizontal[];
PRINTING_EXPORT extern const char kSettingDpiVertical[];
PRINTING_EXPORT extern const char kSettingDuplexMode[];
PRINTING_EXPORT extern const char kSettingFitToPageScaling[];
PRINTING_EXPORT extern const char kSettingHeaderFooterEnabled[];
PRINTING_EXPORT extern const float kSettingHeaderFooterInterstice;
PRINTING_EXPORT extern const char kSettingHeaderFooterDate[];
PRINTING_EXPORT extern const char kSettingHeaderFooterTitle[];
PRINTING_EXPORT extern const char kSettingHeaderFooterURL[];
PRINTING_EXPORT extern const char kSettingLandscape[];
PRINTING_EXPORT extern const char kSettingMediaSize[];
PRINTING_EXPORT extern const char kSettingMediaSizeHeightMicrons[];
PRINTING_EXPORT extern const char kSettingMediaSizeWidthMicrons[];
PRINTING_EXPORT extern const char kSettingMediaSizeVendorId[];
PRINTING_EXPORT extern const char kSettingMediaSizeIsDefault[];
PRINTING_EXPORT extern const char kSettingMarginBottom[];
PRINTING_EXPORT extern const char kSettingMarginLeft[];
PRINTING_EXPORT extern const char kSettingMarginRight[];
PRINTING_EXPORT extern const char kSettingMarginTop[];
PRINTING_EXPORT extern const char kSettingMarginsCustom[];
PRINTING_EXPORT extern const char kSettingMarginsType[];
PRINTING_EXPORT extern const char kSettingPreviewPageCount[];
PRINTING_EXPORT extern const char kSettingPageRange[];
PRINTING_EXPORT extern const char kSettingPageRangeFrom[];
PRINTING_EXPORT extern const char kSettingPageRangeTo[];
PRINTING_EXPORT extern const char kSettingPageWidth[];
PRINTING_EXPORT extern const char kSettingPageHeight[];
PRINTING_EXPORT extern const char kSettingPagesPerSheet[];
PRINTING_EXPORT extern const char kSettingPinValue[];
PRINTING_EXPORT extern const char kSettingPolicies[];
PRINTING_EXPORT extern const char kSettingPreviewIsFromArc[];
PRINTING_EXPORT extern const char kSettingPreviewIsPdf[];
PRINTING_EXPORT extern const char kSettingPreviewModifiable[];
PRINTING_EXPORT extern const char kSettingPrintToGoogleDrive[];
PRINTING_EXPORT extern const char kSettingPrintableAreaHeight[];
PRINTING_EXPORT extern const char kSettingPrintableAreaWidth[];
PRINTING_EXPORT extern const char kSettingPrintableAreaX[];
PRINTING_EXPORT extern const char kSettingPrintableAreaY[];
PRINTING_EXPORT extern const char kSettingPrinterDescription[];
PRINTING_EXPORT extern const char kSettingPrinterName[];
PRINTING_EXPORT extern const char kSettingPrinterOptions[];
PRINTING_EXPORT extern const char kSettingPrinterType[];
PRINTING_EXPORT extern const char kSettingRasterizePdf[];
PRINTING_EXPORT extern const char kSettingScaleFactor[];
PRINTING_EXPORT extern const char kSettingScalingType[];
PRINTING_EXPORT extern const char kSettingTicket[];
PRINTING_EXPORT extern const char kSettingSendUserInfo[];
PRINTING_EXPORT extern const char kSettingShouldPrintBackgrounds[];
PRINTING_EXPORT extern const char kSettingShouldPrintSelectionOnly[];
PRINTING_EXPORT extern const char kSettingShowSystemDialog[];
PRINTING_EXPORT extern const char kSettingUsername[];
PRINTING_EXPORT extern const char kSettingAdvancedSettings[];

PRINTING_EXPORT extern const int FIRST_PAGE_INDEX;
PRINTING_EXPORT extern const int COMPLETE_PREVIEW_DOCUMENT_INDEX;
PRINTING_EXPORT extern const char kSettingOpenPDFInPreview[];

PRINTING_EXPORT extern const uint32_t kInvalidPageIndex;
PRINTING_EXPORT extern const uint32_t kMaxPageCount;

#if defined(USE_CUPS)
// Printer color models
PRINTING_EXPORT extern const char kBlack[];
PRINTING_EXPORT extern const char kCMYK[];
PRINTING_EXPORT extern const char kKCMY[];
PRINTING_EXPORT extern const char kCMY_K[];
PRINTING_EXPORT extern const char kCMY[];
PRINTING_EXPORT extern const char kColor[];
PRINTING_EXPORT extern const char kEpsonColor[];
PRINTING_EXPORT extern const char kEpsonMono[];
PRINTING_EXPORT extern const char kFullColor[];
PRINTING_EXPORT extern const char kGray[];
PRINTING_EXPORT extern const char kGrayscale[];
PRINTING_EXPORT extern const char kGreyscale[];
PRINTING_EXPORT extern const char kMono[];
PRINTING_EXPORT extern const char kMonochrome[];
PRINTING_EXPORT extern const char kNormal[];
PRINTING_EXPORT extern const char kNormalGray[];
PRINTING_EXPORT extern const char kRGB[];
PRINTING_EXPORT extern const char kRGBA[];
PRINTING_EXPORT extern const char kRGB16[];
PRINTING_EXPORT extern const char kSharpCMColor[];
PRINTING_EXPORT extern const char kSharpCMBW[];
PRINTING_EXPORT extern const char kXeroxAutomatic[];
PRINTING_EXPORT extern const char kXeroxBW[];
#endif

// Specifies the horizontal alignment of the headers and footers.
enum HorizontalHeaderFooterPosition { LEFT, CENTER, RIGHT };

// Specifies the vertical alignment of the Headers and Footers.
enum VerticalHeaderFooterPosition { TOP, BOTTOM };

// Must match print_preview.ScalingType in
// chrome/browser/resources/print_preview/data/scaling.js
enum ScalingType {
  DEFAULT,
  FIT_TO_PAGE,
  FIT_TO_PAPER,
  CUSTOM,
  SCALING_TYPE_LAST = CUSTOM
};

// Must match print_preview.PrinterType in
// chrome/browser/resources/print_preview/data/destination_match.js
enum class PrinterType { kPrivet, kExtension, kPdf, kLocal, kCloud };

}  // namespace printing

#endif  // PRINTING_PRINT_JOB_CONSTANTS_H_
