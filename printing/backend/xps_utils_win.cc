// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/xps_utils_win.h"

#include <utility>

#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"

namespace printing {

namespace {

// Elements and namespaces in XML data. The order of these elements follows
// the Print Schema Framework elements order. Details can be found here:
// https://docs.microsoft.com/en-us/windows/win32/printdocs/details-of-the-printcapabilities-schema
constexpr char kPrintCapabilities[] = "psf:PrintCapabilities";
constexpr char kFeature[] = "psf:Feature";
constexpr char kPageOutputQuality[] = "psk:PageOutputQuality";
constexpr char kOption[] = "psf:Option";
constexpr char kProperty[] = "psf:Property";
constexpr char kValue[] = "psf:Value";
constexpr char kName[] = "name";

base::expected<PageOutputQuality, mojom::ResultCode> LoadPageOutputQuality(
    const base::Value& page_output_quality) {
  PageOutputQuality printer_page_output_quality;
  std::vector<const base::Value*> options;
  data_decoder::GetAllXmlElementChildrenWithTag(page_output_quality, kOption,
                                                &options);
  if (options.empty()) {
    LOG(WARNING) << "Incorrect XML format";
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  for (const auto* option : options) {
    PageOutputQualityAttribute quality;
    quality.name = data_decoder::GetXmlElementAttribute(*option, kName);
    int property_count =
        data_decoder::GetXmlElementChildrenCount(*option, kProperty);

    // TODO(crbug.com/40212677): Each formatted option is expected to have zero
    // or one property. Each property inside an option is expected to
    // have one value.
    // Source:
    // https://docs.microsoft.com/en-us/windows/win32/printdocs/pageoutputquality
    // If an option has more than one property or a property has more than one
    // value, more work is expected here.

    // In the case an option looks like <psf:Option name="psk:Text />,
    // property_count is 0. In this case, an option only has `name`
    // and does not have `display_name`.
    if (property_count > 1) {
      LOG(WARNING) << "Incorrect XML format";
      return base::unexpected(mojom::ResultCode::kFailed);
    }
    if (property_count == 1) {
      const base::Value* property_element = data_decoder::FindXmlElementPath(
          *option, {kOption, kProperty}, /*unique_path=*/nullptr);
      int value_count =
          data_decoder::GetXmlElementChildrenCount(*property_element, kValue);
      if (value_count != 1) {
        LOG(WARNING) << "Incorrect XML format";
        return base::unexpected(mojom::ResultCode::kFailed);
      }
      const base::Value* value_element = data_decoder::FindXmlElementPath(
          *option, {kOption, kProperty, kValue}, /*unique_path=*/nullptr);
      std::string text;
      data_decoder::GetXmlElementText(*value_element, &text);
      quality.display_name = std::move(text);
    }
    printer_page_output_quality.qualities.push_back(std::move(quality));
  }
  return printer_page_output_quality;
}

}  // namespace

base::expected<XpsCapabilities, mojom::ResultCode>
ParseValueForXpsPrinterCapabilities(const base::Value& capabilities) {
  if (!data_decoder::IsXmlElementNamed(capabilities, kPrintCapabilities)) {
    LOG(WARNING) << "Incorrect XML format";
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  std::vector<const base::Value*> features;
  data_decoder::GetAllXmlElementChildrenWithTag(capabilities, kFeature,
                                                &features);
  if (features.empty()) {
    LOG(WARNING) << "Incorrect XML format";
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  XpsCapabilities xps_capabilities;
  for (auto* feature : features) {
    std::string feature_name =
        data_decoder::GetXmlElementAttribute(*feature, kName);
    DVLOG(2) << feature_name;
    if (feature_name == kPageOutputQuality) {
      ASSIGN_OR_RETURN(xps_capabilities.page_output_quality,
                       LoadPageOutputQuality(*feature));
    }

    // TODO(crbug.com/40212677): Each feature needs to be parsed. More work is
    // expected here.
  }
  return xps_capabilities;
}

void MergeXpsCapabilities(
    XpsCapabilities xps_capabilities,
    PrinterSemanticCapsAndDefaults& printer_capabilities) {
  printer_capabilities.page_output_quality =
      std::move(xps_capabilities.page_output_quality);
}

}  // namespace printing
