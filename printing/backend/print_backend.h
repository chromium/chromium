// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_H_
#define PRINTING_BACKEND_PRINT_BACKEND_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/printing_export.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class DictionaryValue;
}

// This is the interface for platform-specific code for a print backend
namespace printing {

struct PRINTING_EXPORT PrinterBasicInfo {
  PrinterBasicInfo();
  PrinterBasicInfo(const PrinterBasicInfo& other);
  ~PrinterBasicInfo();

  // The name of the printer as understood by OS.
  std::string printer_name;

  // The name of the printer as shown in Print Preview.
  // For Windows SetGetDisplayNameFunction() can be used to set the setter of
  // this field.
  std::string display_name;
  std::string printer_description;
  int printer_status = 0;
  int is_default = false;
  std::map<std::string, std::string> options;
};

using PrinterList = std::vector<PrinterBasicInfo>;

#if defined(OS_CHROMEOS)

struct PRINTING_EXPORT AdvancedCapabilityValue {
  AdvancedCapabilityValue();
  AdvancedCapabilityValue(const AdvancedCapabilityValue& other);
  ~AdvancedCapabilityValue();

  // IPP identifier of the value.
  std::string name;

  // Localized name for the value.
  std::string display_name;
};

struct PRINTING_EXPORT AdvancedCapability {
  AdvancedCapability();
  AdvancedCapability(const AdvancedCapability& other);
  ~AdvancedCapability();

  enum class Type : uint8_t { kBoolean, kFloat, kInteger, kString };

  // IPP identifier of the attribute.
  std::string name;

  // Localized name for the attribute.
  std::string display_name;

  // Attribute type.
  AdvancedCapability::Type type;

  // Default value.
  std::string default_value;

  // Values for enumerated attributes.
  std::vector<AdvancedCapabilityValue> values;
};

using AdvancedCapabilities = std::vector<AdvancedCapability>;

#endif  // defined(OS_CHROMEOS)

struct PRINTING_EXPORT PrinterSemanticCapsAndDefaults {
  PrinterSemanticCapsAndDefaults();
  PrinterSemanticCapsAndDefaults(const PrinterSemanticCapsAndDefaults& other);
  ~PrinterSemanticCapsAndDefaults();

  bool collate_capable = false;
  bool collate_default = false;

  // If |copies_max| > 1, copies are supported.
  // If |copies_max| = 1, copies are not supported.
  // |copies_max| should never be < 1.
  int32_t copies_max = 1;

  std::vector<mojom::DuplexMode> duplex_modes;
  mojom::DuplexMode duplex_default = mojom::DuplexMode::kUnknownDuplexMode;

  bool color_changeable = false;
  bool color_default = false;
  mojom::ColorModel color_model = mojom::ColorModel::kUnknownColorModel;
  mojom::ColorModel bw_model = mojom::ColorModel::kUnknownColorModel;

  struct PRINTING_EXPORT Paper {
    std::string display_name;
    std::string vendor_id;
    gfx::Size size_um;

    bool operator==(const Paper& other) const;
  };
  using Papers = std::vector<Paper>;
  Papers papers;
  Papers user_defined_papers;
  Paper default_paper;

  std::vector<gfx::Size> dpis;
  gfx::Size default_dpi;

#if defined(OS_CHROMEOS)
  bool pin_supported = false;
  AdvancedCapabilities advanced_capabilities;
#endif  // defined(OS_CHROMEOS)
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

  // Gets the basic printer info for a specific printer. Implementations must
  // check |printer_name| validity in the same way as IsValidPrinter().
  virtual bool GetPrinterBasicInfo(const std::string& printer_name,
                                   PrinterBasicInfo* printer_info) = 0;

  // Gets the semantic capabilities and defaults for a specific printer.
  // This is usually a lighter implementation than GetPrinterCapsAndDefaults().
  // Implementations must check |printer_name| validity in the same way as
  // IsValidPrinter().
  // NOTE: on some old platforms (WinXP without XPS pack)
  // GetPrinterCapsAndDefaults() will fail, while this function will succeed.
  virtual bool GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) = 0;

  // Gets the capabilities and defaults for a specific printer.
  virtual bool GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) = 0;

  // Gets the information about driver for a specific printer.
  virtual std::string GetPrinterDriverInfo(const std::string& printer_name) = 0;

  // Returns true if printer_name points to a valid printer.
  virtual bool IsValidPrinter(const std::string& printer_name) = 0;

  // Allocates a print backend.
  static scoped_refptr<PrintBackend> CreateInstance(const std::string& locale);

#if defined(USE_CUPS)
  // TODO(crbug.com/1062136): Remove this static function when Cloud Print is
  // supposed to stop working. Follow up after Jan 1, 2021.
  // Similar to CreateInstance(), but ensures that the CUPS PPD backend is used
  // instead of the CUPS IPP backend.
  static scoped_refptr<PrintBackend> CreateInstanceForCloudPrint(
      const base::DictionaryValue* print_backend_settings);
#endif  // defined(USE_CUPS)

  // Test method to override the print backend for testing.  Caller should
  // retain ownership.
  static void SetPrintBackendForTesting(PrintBackend* print_backend);

 protected:
  friend class base::RefCountedThreadSafe<PrintBackend>;
  explicit PrintBackend(const std::string& locale);
  virtual ~PrintBackend();

  // Provide the actual backend for CreateInstance().
  static scoped_refptr<PrintBackend> CreateInstanceImpl(
      const base::DictionaryValue* print_backend_settings,
      const std::string& locale,
      bool for_cloud_print);

  const std::string& locale() const { return locale_; }

 private:
  const std::string locale_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_H_
