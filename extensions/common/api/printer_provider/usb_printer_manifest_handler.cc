// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/printer_provider/usb_printer_manifest_handler.h"

#include "extensions/common/api/printer_provider/usb_printer_manifest_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

UsbPrinterManifestHandler::UsbPrinterManifestHandler() {
}

UsbPrinterManifestHandler::~UsbPrinterManifestHandler() {
}

bool UsbPrinterManifestHandler::Parse(Extension* extension,
                                      std::u16string* error) {
  const base::Value* usb_printers =
      extension->manifest()->FindPath(manifest_keys::kUsbPrinters);
  CHECK(usb_printers != nullptr);
  std::unique_ptr<UsbPrinterManifestData> data =
      UsbPrinterManifestData::FromValue(*usb_printers, error);
  if (!data) {
    return false;
  }

  extension->SetManifestData(manifest_keys::kUsbPrinters, std::move(data));
  return true;
}

base::span<const char* const> UsbPrinterManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kUsbPrinters};
  return kKeys;
}

}  // namespace extensions
