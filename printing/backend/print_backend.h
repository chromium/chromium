// Copyright 2012 The Chromium Authors
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
#include "base/values.h"
#include "build/build_config.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include "base/types/expected.h"
#endif  // BUILDFLAG(IS_WIN)

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

#if BUILDFLAG(IS_WIN)

struct COMPONENT_EXPORT(PRINT_BACKEND) PageOutputQualityAttribute {
  PageOutputQualityAttribute();
  PageOutputQualityAttribute(const std::string& display_name,
                             const std::string& name);
  ~PageOutputQualityAttribute();

  bool operator==(const PageOutputQualityAttribute& other) const;

  bool operator<(const PageOutputQualityAttribute& other) const;

  // Localized name of the page output quality attribute.
  std::string display_name;

  // Internal ID of the page output quality attribute.
  std::string name;
};
using PageOutputQualityAttributes = std::vector<PageOutputQualityAttribute>;

struct COMPONENT_EXPORT(PRINT_BACKEND) PageOutputQuality {
  PageOutputQuality();
  PageOutputQuality(PageOutputQualityAttributes qualities,
                    absl::optional<std::string> default_quality);
  PageOutputQuality(const PageOutputQuality& other);
  ~PageOutputQuality();

  // All options of page output quality.
  PageOutputQualityAttributes qualities;

  // Default option of page output quality.
  // TODO(crbug.com/1291257): Need populate this option in the next CLs.
  absl::optional<std::string> default_quality;
};

#if defined(UNIT_TEST)

COMPONENT_EXPORT(PRINT_BACKEND)
bool operator==(const PageOutputQuality& quality1,
                const PageOutputQuality& quality2);

#endif  // defined(UNIT_TEST)

struct COMPONENT_EXPORT(PRINT_BACKEND) XpsCapabilities {
  XpsCapabilities();
  XpsCapabilities(const XpsCapabilities&) = delete;
  XpsCapabilities& operator=(const XpsCapabilities&) = delete;
  XpsCapabilities(XpsCapabilities&& other) noexcept;
  XpsCapabilities& operator=(XpsCapabilities&& other) noexcept;
  ~XpsCapabilities();

  absl::optional<PageOutputQuality> page_output_quality;
};

#endif  // BUILDFLAG(IS_WIN)

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

    // Origin (x,y) is at the bottom-left.
    gfx::Rect printable_area_um;

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

#if BUILDFLAG(IS_WIN)
  absl::optional<PageOutputQuality> page_output_quality;
#endif  // BUILDFLAG(IS_WIN)
};

#if defined(UNIT_TEST)

COMPONENT_EXPORT(PRINT_BACKEND)
bool operator==(const PrinterSemanticCapsAndDefaults& caps1,
                const PrinterSemanticCapsAndDefaults& caps2);

#endif  // defined(UNIT_TEST)

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
  // into `printer_list`.  Note that `printer_list` should be empty prior to
  // this call.  If there are no printers installed then it will still return
  // success, and `printer_list` remains empty.  The result code will return
  // one of the error result codes when there is a failure in generating the
  // list.
  virtual mojom::ResultCode EnumeratePrinters(PrinterList& printer_list) = 0;

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

#if BUILDFLAG(IS_WIN)
  // Gets the capabilities and defaults for a specific printer.
  // TODO(crbug.com/1008222): Evaluate if this code is useful and delete if not.
  virtual mojom::ResultCode GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) = 0;

  // Gets the printable area for just a single paper size.  Returns nullopt if
  // there is any error in retrieving this data.
  // TODO(crbug.com/1424368):  Remove this if the printable areas can be made
  // fully available from `GetPrinterSemanticCapsAndDefaults()`.
  virtual absl::optional<gfx::Rect> GetPaperPrintableArea(
      const std::string& printer_name,
      const std::string& paper_vendor_id,
      const gfx::Size& paper_size_um) = 0;
#endif

  // Gets the information about driver for a specific printer.
  virtual std::string GetPrinterDriverInfo(const std::string& printer_name) = 0;

  // Returns true if printer_name points to a valid printer.
  virtual bool IsValidPrinter(const std::string& printer_name) = 0;

#if BUILDFLAG(IS_WIN)

  // This method uses the XPS API to get the printer capabilities.
  // Returns raw XML string on success, or mojom::ResultCode on failure.
  // This method is virtual to support testing.
  virtual base::expected<std::string, mojom::ResultCode>
  GetXmlPrinterCapabilitiesForXpsDriver(const std::string& printer_name);

#endif  // BUILDFLAG(IS_WIN)

  // Allocates a print backend.
  static scoped_refptr<PrintBackend> CreateInstance(const std::string& locale);

  // Test method to override the print backend for testing.  Caller should
  // retain ownership.
  static void SetPrintBackendForTesting(PrintBackend* print_backend);

 protected:
  friend class base::RefCountedThreadSafe<PrintBackend>;

  PrintBackend();
  virtual ~PrintBackend();

  // Provide the actual backend for CreateInstance().
  static scoped_refptr<PrintBackend> CreateInstanceImpl(
      const base::Value::Dict* print_backend_settings,
      const std::string& locale);
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_H_
