// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_PRINTER_PROVIDER_USB_PRINTER_MANIFEST_HANDLER_H_
#define EXTENSIONS_COMMON_API_PRINTER_PROVIDER_USB_PRINTER_MANIFEST_HANDLER_H_

#include "extensions/common/manifest_handler.h"

namespace extensions {

// Parses the "usb_printers" manifest key.
class UsbPrinterManifestHandler : public ManifestHandler {
 public:
  UsbPrinterManifestHandler();
  ~UsbPrinterManifestHandler() override;

 private:
  // ManifestHandler overrides.
  bool Parse(Extension* extension, std::u16string* error) override;
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_PRINTER_PROVIDER_USB_PRINTER_MANIFEST_HANDLER_H_
