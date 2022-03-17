// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_H_
#define PRINTING_BACKEND_PRINT_BACKEND_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "printing/mojom/print.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class DictionaryValue;
class Value;
}

// This is the interface for platform-specific code for a print backend
namespace printing {

using PrinterBasicInfoOptions = std::map<std::string, std::string>;

struct COMPONENT_EXPORT(PRINT_BACKEND) PrinterBasicInfo {
  PrinterBasicInfo();
  PrinterBasicInfo(const std::string& printer_name,
                   const std::string& display_name,
                   const std::string& printer_description,
                   int printer_status,
                   bool is_default,
                   const PrinterBasicInfoOptions& options);
  PrinterBasicInfo(const PrinterBasicInfo& other);
  ~PrinterBasicInfo();

  bool operator==(const PrinterBasicInfo& other) const;

  // The name of the printer as understood by OS.
  std::string printer_name;

  // The name of the printer as shown in Print Preview.
  // For Windows SetGetDisplayNameFunction() can be used to set the setter of
  // this field.
  std::string display_name;
  std::string printer_description;
  int printer_status = 0;
  bool is_default = false;
  PrinterBasicInfoOptions options;
};

using PrinterList = std::vector<PrinterBasicInfo>;

#if BUILDFLAG(IS_CHROMEOS)

struct COMPONENT_EXPORT(PRINT_BACKEND) AdvancedCapabilityValue {
  AdvancedCapabilityValue();
  AdvancedCapabilityValue(const std::string& name,
                          const std::string& display_name);
  AdvancedCapabilityValue(const AdvancedCapabilityValue& other);
  ~AdvancedCapabilityValue();

  bool operator==(const AdvancedCapabilityValue& other) const;

  // IPP identifier of the value.
  std::string name;

  // Localized name for the value.
  std::string display_name;
};

struct COMPONENT_EXPORT(PRINT_BACKEND) AdvancedCapability {
  enum class Type : uint8_t { kBoolean, kFloat, kInteger, kString };

  AdvancedCapability();
  AdvancedCapability(const std::string& name, AdvancedCapability::Type type);
  AdvancedCapability(const std::string& name,
                     const std::string& display_name,
                     AdvancedCapability::Type type,
                     const std::string& default_value,
                     const std::vector<AdvancedCapabilityValue>& values);
  AdvancedCapability(const AdvancedCapability& other);
  ~AdvancedCapability();

  bool operator==(const AdvancedCapability& other) const;

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

#endif  // BUILDFLAG(IS_CHROMEOS)

struct COMPONENT_EXPORT(PRINT_BACKEND) PrinterSemanticCapsAndDefaults {
  PrinterSemanticCapsAndDefaults();
  PrinterSemanticCapsAndDefaults(const PrinterSemanticCapsAndDefaults& other);
  ~PrinterSemanticCapsAndDefaults();

  bool collate_capable = false;
  bool collate_default = false;

  // If `copies_max` > 1, copies are supported.
  // If `copies_max` = 1, copies are not supported.
  // `copies_max` should never be < 1.
  int32_t copies_max = 1;

  std::vector<mojom::DuplexMode> duplex_modes;
  mojom::DuplexMode duplex_default = mojom::DuplexMode::kUnknownDuplexMode;

  bool color_changeable = false;
  bool color_default = false;
  mojom::ColorModel color_model = mojom::ColorModel::kUnknownColorModel;
  mojom::ColorModel bw_model = mojom::ColorModel::kUnknownColorModel;

  struct COMPONENT_EXPORT(PRINT_BACKEND) Paper {
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

#if BUILDFLAG(IS_CHROMEOS)
  bool pin_supported = false;
  AdvancedCapabilities advanced_capabilities;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

struct COMPONENT_EXPORT(PRINT_BACKEND) PrinterCapsAndDefaults {
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
class COMPONENT_EXPORT(PRINT_BACKEND) PrintBackend
    : public base::RefCountedThreadSafe<PrintBackend> {
 public:
  // Enumerates the list of installed local and network printers.  It will
  // return success when the available installed printers have been enumerated
  // into `printer_list`.  Note that `printer_list` must not be null and also
  // should be empty prior to this call.  If there are no printers installed
  // then it will still return success, and `printer_list` remains empty.  The
  // result code will return one of the error result codes when there is a
  // failure in generating the list.
  virtual mojom::ResultCode EnumeratePrinters(PrinterList* printer_list) = 0;

  // Gets the default printer name.  If there is no default printer then it
  // will still return success and `default_printer` will be empty.  The result
  // code will return one of the error result codes when there is a failure in
  // trying to get the default printer.
  virtual mojom::ResultCode GetDefaultPrinterName(
      std::string& default_printer) = 0;

  // Gets the basic printer info for a specific printer. Implementations must
  // check `printer_name` validity in the same way as IsValidPrinter().
  virtual mojom::ResultCode GetPrinterBasicInfo(
      const std::string& printer_name,
      PrinterBasicInfo* printer_info) = 0;

  // Gets the semantic capabilities and defaults for a specific printer.
  // This is usually a lighter implementation than GetPrinterCapsAndDefaults().
  // Implementations must check `printer_name` validity in the same way as
  // IsValidPrinter().
  // NOTE: on some old platforms (WinXP without XPS pack)
  // GetPrinterCapsAndDefaults() will fail, while this function will succeed.
  virtual mojom::ResultCode GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) = 0;

  // Gets the capabilities and defaults for a specific printer.
  virtual mojom::ResultCode GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) = 0;

  // Gets the information about driver for a specific printer.
  virtual std::string GetPrinterDriverInfo(const std::string& printer_name) = 0;

  // Returns true if printer_name points to a valid printer.
  virtual bool IsValidPrinter(const std::string& printer_name) = 0;

  // Allocates a print backend.
  static scoped_refptr<PrintBackend> CreateInstance(const std::string& locale);

  // Test method to override the print backend for testing.  Caller should
  // retain ownership.
  static void SetPrintBackendForTesting(PrintBackend* print_backend);

 protected:
  friend class base::RefCountedThreadSafe<PrintBackend>;

#if BUILDFLAG(IS_WIN)
  FRIEND_TEST_ALL_PREFIXES(PrintBackendTest,
                           MANUAL_GetXmlPrinterCapabilitiesForXpsDriver);
  FRIEND_TEST_ALL_PREFIXES(PrintBackendTest,
                           ParseValueForXpsPrinterCapabilities);
#endif

  PrintBackend();
  virtual ~PrintBackend();

  // Provide the actual backend for CreateInstance().
  static scoped_refptr<PrintBackend> CreateInstanceImpl(
      const base::DictionaryValue* print_backend_settings,
      const std::string& locale);

#if BUILDFLAG(IS_WIN)
  // Gets the semantic capabilities and defaults for a specific printer.
  // This method uses the XPS API to get the printer capabilities.
  // TODO(crbug.com/1291257): This method is not fully implemented yet.
  mojom::ResultCode GetXmlPrinterCapabilitiesForXpsDriver(
      const std::string& printer_name,
      std::string& capabilities);
  mojom::ResultCode ParseValueForXpsPrinterCapabilities(
      const base::Value& value,
      PrinterSemanticCapsAndDefaults* printer_info);
#endif
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_H_
