// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_H_
#define PRINTING_BACKEND_PRINT_BACKEND_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "printing/print_job_constants.h"
#include "printing/printing_export.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class DictionaryValue;
}

// This is the interface for platform-specific code for a print backend
namespace printing {

// Note: There are raw values. The |printer_name| and |printer_description|
// require further interpretation on Windows, Mac and Chrome OS. See existing
// callers for examples.
struct PRINTING_EXPORT PrinterBasicInfo {
  PrinterBasicInfo();
  PrinterBasicInfo(const PrinterBasicInfo& other);
  ~PrinterBasicInfo();

  std::string printer_name;
  std::string printer_description;
  int printer_status;
  int is_default;
  std::map<std::string, std::string> options;
};

using PrinterList = std::vector<PrinterBasicInfo>;

struct PRINTING_EXPORT PrinterSemanticCapsAndDefaults {
  PrinterSemanticCapsAndDefaults();
  PrinterSemanticCapsAndDefaults(const PrinterSemanticCapsAndDefaults& other);
  ~PrinterSemanticCapsAndDefaults();

  bool collate_capable;
  bool collate_default;

  bool copies_capable;

  std::vector<DuplexMode> duplex_modes;
  DuplexMode duplex_default;

  bool color_changeable;
  bool color_default;
  ColorModel color_model;
  ColorModel bw_model;

  struct Paper {
    std::string display_name;
    std::string vendor_id;
    gfx::Size size_um;
  };
  std::vector<Paper> papers;
  Paper default_paper;

  std::vector<gfx::Size> dpis;
  gfx::Size default_dpi;
};

struct PRINTING_EXPORT PrinterCapsAndDefaults {
  PrinterCapsAndDefaults();
  PrinterCapsAndDefaults(const PrinterCapsAndDefaults& other);
  ~PrinterCapsAndDefaults();

  std::string printer_capabilities;
  std::string caps_mime_type;
  std::string printer_defaults;
  std::string defaults_mime_type;
};

// PrintBackend class will provide interface for different print backends
// (Windows, CUPS) to implement. User will call CreateInstance() to
// obtain available print backend.
// Please note, that PrintBackend is not platform specific, but rather
// print system specific. For example, CUPS is available on both Linux and Mac,
// but not available on ChromeOS, etc. This design allows us to add more
// functionality on some platforms, while reusing core (CUPS) functions.
class PRINTING_EXPORT PrintBackend
    : public base::RefCountedThreadSafe<PrintBackend> {
 public:
  // Enumerates the list of installed local and network printers.
  virtual bool EnumeratePrinters(PrinterList* printer_list) = 0;

  // Gets the default printer name. Empty string if no default printer.
  virtual std::string GetDefaultPrinterName() = 0;

  // Gets the basic printer info for a specific printer.
  virtual bool GetPrinterBasicInfo(const std::string& printer_name,
                                   PrinterBasicInfo* printer_info) = 0;

  // Gets the semantic capabilities and defaults for a specific printer.
  // This is usually a lighter implementation than GetPrinterCapsAndDefaults().
  // NOTE: on some old platforms (WinXP without XPS pack)
  // GetPrinterCapsAndDefaults() will fail, while this function will succeed.
  virtual bool GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) = 0;

#if !defined(OS_CHROMEOS)
  // Gets the capabilities and defaults for a specific printer.
  virtual bool GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) = 0;
#endif  // !defined(OS_CHROMEOS)

  // Gets the information about driver for a specific printer.
  virtual std::string GetPrinterDriverInfo(
      const std::string& printer_name) = 0;

  // Returns true if printer_name points to a valid printer.
  virtual bool IsValidPrinter(const std::string& printer_name) = 0;

  // Allocates a print backend. If |print_backend_settings| is nullptr, default
  // settings will be used.
  static scoped_refptr<PrintBackend> CreateInstance(
      const base::DictionaryValue* print_backend_settings);

  // Test method to override the print backend for testing.  Caller should
  // retain ownership.
  static void SetPrintBackendForTesting(PrintBackend* print_backend);

 protected:
  friend class base::RefCountedThreadSafe<PrintBackend>;
  virtual ~PrintBackend();

  // Provide the actual backend for CreateInstance().
  static scoped_refptr<PrintBackend> CreateInstanceImpl(
      const base::DictionaryValue* print_backend_settings);
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_H_
