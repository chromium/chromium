// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_WIN_H_
#define PRINTING_BACKEND_PRINT_BACKEND_WIN_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "printing/backend/print_backend.h"
#include "ui/gfx/geometry/rect.h"

namespace printing {

class COMPONENT_EXPORT(PRINT_BACKEND) PrintBackendWin : public PrintBackend {
 public:
  PrintBackendWin();

  // PrintBackend implementation.
  mojom::ResultCode EnumeratePrinters(PrinterList& printer_list) override;
  mojom::ResultCode GetDefaultPrinterName(
      std::string& default_printer) override;
  mojom::ResultCode GetPrinterBasicInfo(
      const std::string& printer_name,
      PrinterBasicInfo* printer_info) override;
  mojom::ResultCode GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) override;
  mojom::ResultCode GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) override;
  std::optional<gfx::Rect> GetPaperPrintableArea(
      const std::string& printer_name,
      const std::string& paper_vendor_id,
      const gfx::Size& paper_size_um) override;
  std::vector<std::string> GetPrinterDriverInfo(
      const std::string& printer_name) override;
  bool IsValidPrinter(const std::string& printer_name) override;

  // Allow testing to install a hook to be notified whenever the printable area
  // is requested from the operating system.
  void SetPrintableAreaLoadedCallbackForTesting(
      base::RepeatingClosure callback);

 protected:
  // The printable area of a paper and the inputs needed to query for it with
  // `GetPaperPrintableArea()`.
  struct DriverPaperPrintableArea {
    DriverPaperPrintableArea(const std::string& name,
                             const std::vector<std::string>& driver,
                             const std::string& id,
                             const gfx::Rect& area_um);
    DriverPaperPrintableArea(const DriverPaperPrintableArea& other);
    ~DriverPaperPrintableArea();

    // Identify the printer and driver info this was saved for.
    std::string printer_name;
    std::vector<std::string> driver_info;

    // Platform specific id to map it back to the particular media.
    std::string vendor_id;

    gfx::Rect printable_area_um;
  };

  ~PrintBackendWin() override;

 private:
  // A cache of the printable area for the default paper that was loaded by
  // `GetPrinterSemanticCapsAndDefaults()`. The value remains cached until it is
  // replaced by another call to `GetPrinterSemanticCapsAndDefaults()`.  This
  // ensures its validity matches that of any other printer settings that have
  // been fetched from the operating system and would be used by callers.
  std::optional<DriverPaperPrintableArea> last_default_paper_printable_area_;

  base::RepeatingClosure printable_area_loaded_callback_for_test_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_WIN_H_
