// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_JOB_CONSTANTS_H_
#define PRINTING_PRINT_JOB_CONSTANTS_H_

#include <stdint.h>

#include "base/component_export.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"

namespace printing {

COMPONENT_EXPORT(PRINTING_BASE) extern const char kIsFirstRequest[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kPreviewRequestID[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kPreviewUIID[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSettingBorderless[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingCapabilities[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSettingCollate[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSettingColor[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingSetColorAsDefault[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingContentHeight[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingContentWidth[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSettingCopies[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingDeviceName[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingDisableScaling[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingDpiDefault[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingDpiHorizontal[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingDpiVertical[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingDuplexMode[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingHeaderFooterEnabled[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const float kSettingHeaderFooterInterstice;
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingHeaderFooterDate[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingHeaderFooterTitle[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingHeaderFooterURL[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingLandscape[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMediaSize[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMediaSizeHeightMicrons[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMediaSizeWidthMicrons[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingsImageableAreaLeftMicrons[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingsImageableAreaBottomMicrons[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingsImageableAreaRightMicrons[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingsImageableAreaTopMicrons[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMediaSizeVendorId[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMediaSizeIsDefault[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMediaType[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMarginBottom[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMarginLeft[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMarginRight[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMarginTop[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMarginsCustom[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingMarginsType[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPreviewPageCount[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPageRange[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPageRangeFrom[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPageRangeTo[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPageWidth[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPageHeight[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPagesPerSheet[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSettingPinValue[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSettingPolicies[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPreviewIsFromArc[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPreviewModifiable[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrintToGoogleDrive[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrintableAreaHeight[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrintableAreaWidth[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrintableAreaX[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrintableAreaY[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrinterDescription[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrinterName[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrinterOptions[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrinterType[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingRasterizePdf[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingRasterizePdfDpi[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingScaleFactor[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingScalingType[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSettingTicket[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingSendUserInfo[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingShouldPrintBackgrounds[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingShouldPrintSelectionOnly[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingShowSystemDialog[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSettingUsername[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingAdvancedSettings[];

COMPONENT_EXPORT(PRINTING_BASE) extern const int FIRST_PAGE_INDEX;
COMPONENT_EXPORT(PRINTING_BASE)
extern const int COMPLETE_PREVIEW_DOCUMENT_INDEX;
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingOpenPDFInPreview[];

COMPONENT_EXPORT(PRINTING_BASE)
extern const uint32_t kInvalidPageIndex;
COMPONENT_EXPORT(PRINTING_BASE) extern const uint32_t kMaxPageCount;

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingChromeOSAccessOAuthToken[];

COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingIppClientInfo[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingIppClientName[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingIppClientPatches[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingIppClientStringVersion[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingIppClientType[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingIppClientVersion[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrinterManuallySelected[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kSettingPrinterStatusReason[];
#endif  // BUILDFLAG(IS_CHROMEOS)

// Specifies the horizontal alignment of the headers and footers.
enum HorizontalHeaderFooterPosition { LEFT, CENTER, RIGHT };

// Specifies the vertical alignment of the Headers and Footers.
enum VerticalHeaderFooterPosition { TOP, BOTTOM };

// Must match print_preview.ScalingType in
// chrome/browser/resources/print_preview/data/scaling.ts
enum ScalingType {
  DEFAULT,
  FIT_TO_PAGE,
  FIT_TO_PAPER,
  CUSTOM,
  SCALING_TYPE_LAST = CUSTOM
};

}  // namespace printing

#endif  // PRINTING_PRINT_JOB_CONSTANTS_H_
