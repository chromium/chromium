// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_PRINTER_PROVIDER_USB_PRINTER_MANIFEST_DATA_H_
#define EXTENSIONS_COMMON_API_PRINTER_PROVIDER_USB_PRINTER_MANIFEST_DATA_H_

#include <memory>
#include <vector>

#include "extensions/common/extension.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

namespace extensions {

// The parsed form of the "usb_printers" manifest entry.
class UsbPrinterManifestData : public Extension::ManifestData {
 public:
  UsbPrinterManifestData();
  ~UsbPrinterManifestData() override;

  // Gets the UsbPrinterManifestData for |extension|, or NULL if none was
  // specified.
  static const UsbPrinterManifestData* Get(const Extension* extension);

  // Parses the data stored in |value|. Sets |error| and returns an empty
  // scoped_ptr on failure.
  static std::unique_ptr<UsbPrinterManifestData> FromValue(
      const base::Value& value,
      base::string16* error);

  bool SupportsDevice(const device::mojom::UsbDeviceInfo& device) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(UsbPrinterManifestTest, Filters);

  std::vector<device::mojom::UsbDeviceFilterPtr> filters_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_PRINTER_PROVIDER_USB_PRINTER_MANIFEST_DATA_H_
