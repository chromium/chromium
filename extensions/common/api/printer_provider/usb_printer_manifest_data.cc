// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/printer_provider/usb_printer_manifest_data.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/manifest_constants.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

namespace extensions {

UsbPrinterManifestData::UsbPrinterManifestData() {
}

UsbPrinterManifestData::~UsbPrinterManifestData() {
}

// static
const UsbPrinterManifestData* UsbPrinterManifestData::Get(
    const Extension* extension) {
  return static_cast<UsbPrinterManifestData*>(
      extension->GetManifestData(manifest_keys::kUsbPrinters));
}

// static
std::unique_ptr<UsbPrinterManifestData> UsbPrinterManifestData::FromValue(
    const base::Value& value,
    std::u16string* error) {
  auto usb_printers =
      api::extensions_manifest_types::UsbPrinters::FromValue(value);
  if (!usb_printers.has_value()) {
    *error = std::move(usb_printers).error();
    return nullptr;
  }

  auto result = std::make_unique<UsbPrinterManifestData>();
  for (const auto& input : usb_printers->filters) {
    if (input.product_id && input.interface_class) {
      *error = u"Only one of productId or interfaceClass may be specified.";
      return nullptr;
    }

    auto output = device::mojom::UsbDeviceFilter::New();
    output->has_vendor_id = true;
    output->vendor_id = input.vendor_id;

    if (input.product_id) {
      output->has_product_id = true;
      output->product_id = *input.product_id;
    }

    if (input.interface_class) {
      output->has_class_code = true;
      output->class_code = *input.interface_class;
      if (input.interface_subclass) {
        output->has_subclass_code = true;
        output->subclass_code = *input.interface_subclass;
        if (input.interface_protocol) {
          output->has_protocol_code = true;
          output->protocol_code = *input.interface_protocol;
        }
      }
    }

    result->filters_.push_back(std::move(output));
  }
  return result;
}

bool UsbPrinterManifestData::SupportsDevice(
    const device::mojom::UsbDeviceInfo& device) const {
  for (const auto& filter : filters_) {
    if (device::UsbDeviceFilterMatches(*filter, device))
      return true;
  }

  return false;
}

}  // namespace extensions
