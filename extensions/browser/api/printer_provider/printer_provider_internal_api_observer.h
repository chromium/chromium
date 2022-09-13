// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_INTERNAL_API_OBSERVER_H_
#define EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_INTERNAL_API_OBSERVER_H_

#include <vector>

#include "base/values.h"
#include "extensions/common/api/printer_provider.h"
#include "extensions/common/api/printer_provider_internal.h"

namespace extensions {

class Extension;

// Interface for observing chrome.printerProviderInternal API function calls.
class PrinterProviderInternalAPIObserver {
 public:
  using PrinterInfoVector = std::vector<api::printer_provider::PrinterInfo>;

  // Used by chrome.printerProviderInternal API to report
  // chrome.printerProvider.onGetPrintersRequested result returned by the
  // extension |extension|.
  // |request_id| is the request id passed to the original
  // chrome.printerProvider.onGetPrintersRequested event.
  virtual void OnGetPrintersResult(const Extension* extension,
                                   int request_id,
                                   const PrinterInfoVector& result) = 0;

  // Used by chrome.printerProviderInternal API to report
  // chrome.printerProvider.onGetCapabilityRequested result returned by the
  // extension |extension|.
  // |request_id| is the request id passed to the original
  // chrome.printerProvider.onGetCapabilityRequested event.
  virtual void OnGetCapabilityResult(const Extension* extension,
                                     int request_id,
                                     base::Value::Dict result) = 0;

  // Used by chrome.printerProviderInternal API to report
  // chrome.printerProvider.onPrintRequested result returned by the extension
  // |extension|.
  // |request_id| is the request id passed to the original
  // chrome.printerProvider.onPrintRequested event.
  virtual void OnPrintResult(
      const Extension* extension,
      int request_id,
      api::printer_provider_internal::PrintError error) = 0;

  // Used by chrome.printerProviderInternal API to report
  // chrome.printerProvider.onGetUsbPrinterInfoRequested result returned by the
  // extension |extension|.
  // |request_id| is the request id passed to the original
  // chrome.printerProvider.onGetUsbPrinterInfoRequested event.
  virtual void OnGetUsbPrinterInfoResult(
      const Extension* extension,
      int request_id,
      const api::printer_provider::PrinterInfo* printer_info) = 0;

 protected:
  virtual ~PrinterProviderInternalAPIObserver() {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_INTERNAL_API_OBSERVER_H_
