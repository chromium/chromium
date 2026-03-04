// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[ExternalExtensionType="printerProvider.PrinterInfo"]
typedef object PrinterProviderPrinterInfo;

[instanceOf=Blob]
typedef object Blob;

// Same as in printerProvider.PrintError enum API.
enum PrintError {
  "OK",
  "FAILED",
  "INVALID_TICKET",
  "INVALID_DATA"
};

// printerProviderInternal
// Internal API used to run callbacks passed to chrome.printerProvider API
// events.
// When dispatching a chrome.printerProvider API event, its arguments will be
// massaged in custom bindings so a callback is added. The callback uses
// chrome.printerProviderInternal API to report the event results.
// In order to identify the event for which the callback is called, the event is
// internally dispatched having a requestId argument (which is removed from the
// argument list before the event actually reaches the event listeners). The
// requestId is forwarded to the chrome.printerProviderInternal API functions.
[implemented_in="extensions/browser/api/printer_provider/printer_provider_internal_api.h"]
interface PrinterProviderInternal {
  // Runs callback to printerProvider.onGetPrintersRequested event.
  // |requestId|: Parameter identifying the event instance for which the
  //     callback is run.
  // |printers|: List of printers reported by the extension.
  static undefined reportPrinters(
      long requestId, optional sequence<PrinterProviderPrinterInfo> printers);

  // Runs callback to printerProvider.onUsbAccessGranted event.
  // |requestId|: Parameter identifying the event instance for which the
  //     callback is run.
  // |printerInfo|: Printer information reported by the extension.
  static undefined reportUsbPrinterInfo(
      long requestId, optional PrinterProviderPrinterInfo printerInfo);

  // Runs callback to printerProvider.onGetCapabilityRequested event.
  // |requestId|: Parameter identifying the event instance for which the
  //     callback is run.
  // |error|: The printer capability returned by the extension.
  static undefined reportPrinterCapability(long request_id,
                                           optional object capability);

  // Runs callback to printerProvider.onPrintRequested event.
  // |requestId|: Parameter identifying the event instance for which the
  //     callback is run.
  // |error|: The requested print job result.
  static undefined reportPrintResult(long request_id,
                                     optional PrintError error);

  // Gets information needed to create a print data blob for a print request.
  // The blob will be dispatched to the extension via
  // printerProvider.onPrintRequested event.
  // |requestId|: The request id for the print request for which data is
  //     needed.
  // |Returns|: Callback called with a blob of print data.
  // |PromiseValue|: blob
  [requiredCallback] static Promise<Blob> getPrintData(long requestId);
};

partial interface Browser {
  static attribute PrinterProviderInternal printerProviderInternal;
};
