// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_H_
#define EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
struct PrinterProviderPrintJob;
}

namespace extensions {

// Implements chrome.printerProvider API events.
class PrinterProviderAPI : public KeyedService {
 public:
  using GetPrintersCallback =
      base::RepeatingCallback<void(base::Value::List printers, bool done)>;
  using GetCapabilityCallback =
      base::OnceCallback<void(const base::Value::Dict capability)>;
  using PrintCallback = base::OnceCallback<void(const base::Value& error)>;
  using GetPrinterInfoCallback =
      base::OnceCallback<void(const base::Value::Dict printer_info)>;

  static PrinterProviderAPI* Create(content::BrowserContext* context);

  // Returns generic error string for print request.
  static std::string GetDefaultPrintError();

  ~PrinterProviderAPI() override = default;

  // Requests list of supported printers from extensions implementing
  // chrome.printerProvider API. It dispatches
  // chrome.printerProvider.onGetPrintersRequested event. The callback is
  // called once for every extension handling the event with a list of its
  // supported printers. The printer values reported by an extension are
  // added "extensionId" property that is set to the ID of the extension
  // returning the list and "extensionName" property set to the extension's
  // name.
  // Note that the "id" property of printer values reported by an extension are
  // rewriten as "<extension_id>:<id>" to ensure they are unique across
  // different extensions.
  virtual void DispatchGetPrintersRequested(
      const GetPrintersCallback& callback) = 0;

  // Requests printer capability for a printer with id |printer_id|.
  // |printer_id| should be one of the printer ids reported by |GetPrinters|
  // callback.
  // It dispatches chrome.printerProvider.onGetCapabilityRequested event
  // to the extension that manages the printer (which can be determined from
  // |printer_id| value).
  // |callback| is passed a dictionary value containing printer capabilities as
  // reported by the extension.
  virtual void DispatchGetCapabilityRequested(
      const std::string& printer_id,
      GetCapabilityCallback callback) = 0;

  // It dispatches chrome.printerProvider.onPrintRequested event with the
  // provided print job. The event is dispatched only to the extension that
  // manages printer with id |job.printer_id|.
  // |callback| is passed the print status returned by the extension, and it
  // must not be null.
  virtual void DispatchPrintRequested(PrinterProviderPrintJob job,
                                      PrintCallback callback) = 0;

  // Returns print job associated with the print request with id |request_id|
  // for extension |extension|.
  // It should return NULL if the job for the request does not exist.
  virtual const PrinterProviderPrintJob* GetPrintJob(const Extension* extension,
                                                     int request_id) const = 0;

  // Dispatches a chrome.printerProvider.getUsbPrinterInfo event requesting
  // information about |device_id|. The event is only dispatched to the
  // extension identified by |extension_id|.
  virtual void DispatchGetUsbPrinterInfoRequested(
      const ExtensionId& extension_id,
      const device::mojom::UsbDeviceInfo& device,
      GetPrinterInfoCallback callback) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_API_H_
