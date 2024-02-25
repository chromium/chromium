// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_MAC_H_
#define PRINTING_PRINTING_CONTEXT_MAC_H_

#include <ApplicationServices/ApplicationServices.h>

#include <string>
#include <string_view>

#include "base/memory/raw_ptr_exclusion.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/printing_context.h"

@class NSPrintInfo;

namespace printing {

class COMPONENT_EXPORT(PRINTING) PrintingContextMac : public PrintingContext {
 public:
  PrintingContextMac(Delegate* delegate, ProcessBehavior process_behavior);
  PrintingContextMac(const PrintingContextMac&) = delete;
  PrintingContextMac& operator=(const PrintingContextMac&) = delete;
  ~PrintingContextMac() override;

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  mojom::ResultCode UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  mojom::ResultCode UpdatePrinterSettings(
      const PrinterSettings& printer_settings) override;
  mojom::ResultCode NewDocument(const std::u16string& document_name) override;
  mojom::ResultCode PrintDocument(const MetafilePlayer& metafile,
                                  const PrintSettings& settings,
                                  uint32_t num_pages) override;
  mojom::ResultCode DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  printing::NativeDrawingContext context() const override;

 private:
  // Initializes PrintSettings from `print_info_`. This must be called
  // after changes to `print_info_` in order for the changes to take effect in
  // printing.
  // This function ignores the page range information specified in the print
  // info object and use `settings_.ranges` instead.
  void InitPrintSettingsFromPrintInfo();

  // Returns the set of page ranges constructed from `print_info_`.
  PageRanges GetPageRangesFromPrintInfo();

  // Updates `print_info_` to use the given printer.
  // Returns true if the printer was set.
  bool SetPrinter(const std::string& device_name);

  // Updates `print_info_` page format with paper selected by user. If paper was
  // not selected, default system paper is used.
  // Returns true if the paper was set.
  bool UpdatePageFormatWithPaperInfo();

  // Updates `print_info_` page format with `paper`.
  // Returns true if the paper was set.
  bool UpdatePageFormatWithPaper(PMPaper paper, PMPageFormat page_format);

  // Sets the print job destination type as preview job.
  // Returns true if the print job destination type is set.
  bool SetPrintPreviewJob();

  // Sets `copies` in PMPrintSettings.
  // Returns true if the number of copies is set.
  bool SetCopiesInPrintSettings(int copies);

  // Sets `collate` in PMPrintSettings.
  // Returns true if `collate` is set.
  bool SetCollateInPrintSettings(bool collate);

  // Sets orientation in native print info object.
  // Returns true if the orientation was set.
  bool SetOrientationIsLandscape(bool landscape);

  // Sets duplex mode in PMPrintSettings.
  // Returns true if duplex mode is set.
  bool SetDuplexModeInPrintSettings(mojom::DuplexMode mode);

  // Sets output color mode in PMPrintSettings.
  // Returns true if color mode is set.
  bool SetOutputColor(int color_mode);

  // Sets resolution in PMPrintSettings.
  // Returns true if resolution is set.
  bool SetResolution(const gfx::Size& dpi_size);

  // Sets key-value pair in PMPrintSettings.
  // Returns true is the pair is set.
  bool SetKeyValue(std::string_view key, std::string_view value);

  // Starts a new page.
  mojom::ResultCode NewPage();

  // Closes the printed page.
  mojom::ResultCode PageDone();

  // The native print info object.
  NSPrintInfo* __strong print_info_;

  // The current page's context; only valid between NewPage and PageDone call
  // pairs.
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #addr-of
  RAW_PTR_EXCLUSION CGContextRef context_ = nullptr;
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_MAC_H_
