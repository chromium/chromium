// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_INTERNAL_API_H_

#include <string>

#include "base/observer_list.h"
#include "base/values.h"
#include "extensions/browser/api/printer_provider/printer_provider_internal_api_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/printer_provider_internal.h"

namespace base {
class RefCountedMemory;
}  // namespace base

namespace content {
class BlobHandle;
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
}

namespace extensions {

// Internal API instance. Primarily used to enable observers to watch for when
// printerProviderInternal API functions are called.
class PrinterProviderInternalAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<PrinterProviderInternalAPI>*
  GetFactoryInstance();

  explicit PrinterProviderInternalAPI(content::BrowserContext* browser_context);

  PrinterProviderInternalAPI(const PrinterProviderInternalAPI&) = delete;
  PrinterProviderInternalAPI& operator=(const PrinterProviderInternalAPI&) =
      delete;

  ~PrinterProviderInternalAPI() override;

  void AddObserver(PrinterProviderInternalAPIObserver* observer);
  void RemoveObserver(PrinterProviderInternalAPIObserver* observer);

 private:
  friend class BrowserContextKeyedAPIFactory<PrinterProviderInternalAPI>;
  friend class PrinterProviderInternalReportPrintersFunction;
  friend class PrinterProviderInternalReportPrinterCapabilityFunction;
  friend class PrinterProviderInternalReportPrintResultFunction;
  friend class PrinterProviderInternalReportUsbPrinterInfoFunction;

  // BrowserContextKeyedAPI implementation.
  static const bool kServiceRedirectedInIncognito = true;
  static const char* service_name() { return "PrinterProviderInternal"; }

  // Notifies observers that a printerProvider.onGetPrintersRequested callback
  // has been called. Called from
  // |PrinterProviderInternalReportPrintersFunction|.
  void NotifyGetPrintersResult(
      const Extension* extension,
      int request_id,
      const PrinterProviderInternalAPIObserver::PrinterInfoVector& printers);

  // Notifies observers that a printerProvider.onGetCapabilityRequested callback
  // has been called. Called from
  // |PrinterProviderInternalReportPrinterCapabilityFunction|.
  void NotifyGetCapabilityResult(const Extension* extension,
                                 int request_id,
                                 const base::Value::Dict& capability);

  // Notifies observers that a printerProvider.onPrintRequested callback has
  // been called. Called from
  // |PrinterProviderInternalReportPrintResultFunction|.
  void NotifyPrintResult(const Extension* extension,
                         int request_id,
                         api::printer_provider_internal::PrintError error);

  // Notifies observers that a printerProvider.onGetUsbPrinterInfoRequested
  // callback has been called. Called from
  // |PrinterProviderInternalReportUsbPrinterInfoFunction|.
  void NotifyGetUsbPrinterInfoResult(
      const Extension* extension,
      int request_id,
      const api::printer_provider::PrinterInfo* printer_info);

  base::ObserverList<PrinterProviderInternalAPIObserver>::Unchecked observers_;
};

class PrinterProviderInternalReportPrintResultFunction
    : public ExtensionFunction {
 public:
  PrinterProviderInternalReportPrintResultFunction();

  PrinterProviderInternalReportPrintResultFunction(
      const PrinterProviderInternalReportPrintResultFunction&) = delete;
  PrinterProviderInternalReportPrintResultFunction& operator=(
      const PrinterProviderInternalReportPrintResultFunction&) = delete;

 protected:
  ~PrinterProviderInternalReportPrintResultFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printerProviderInternal.reportPrintResult",
                             PRINTERPROVIDERINTERNAL_REPORTPRINTRESULT)
};

class PrinterProviderInternalReportPrinterCapabilityFunction
    : public ExtensionFunction {
 public:
  PrinterProviderInternalReportPrinterCapabilityFunction();

  PrinterProviderInternalReportPrinterCapabilityFunction(
      const PrinterProviderInternalReportPrinterCapabilityFunction&) = delete;
  PrinterProviderInternalReportPrinterCapabilityFunction& operator=(
      const PrinterProviderInternalReportPrinterCapabilityFunction&) = delete;

 protected:
  ~PrinterProviderInternalReportPrinterCapabilityFunction() override;

  ExtensionFunction::ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printerProviderInternal.reportPrinterCapability",
                             PRINTERPROVIDERINTERNAL_REPORTPRINTERCAPABILITY)
};

class PrinterProviderInternalReportPrintersFunction : public ExtensionFunction {
 public:
  PrinterProviderInternalReportPrintersFunction();

  PrinterProviderInternalReportPrintersFunction(
      const PrinterProviderInternalReportPrintersFunction&) = delete;
  PrinterProviderInternalReportPrintersFunction& operator=(
      const PrinterProviderInternalReportPrintersFunction&) = delete;

 protected:
  ~PrinterProviderInternalReportPrintersFunction() override;
  ExtensionFunction::ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printerProviderInternal.reportPrinters",
                             PRINTERPROVIDERINTERNAL_REPORTPRINTERS)
};

class PrinterProviderInternalGetPrintDataFunction : public ExtensionFunction {
 public:
  PrinterProviderInternalGetPrintDataFunction();

  PrinterProviderInternalGetPrintDataFunction(
      const PrinterProviderInternalGetPrintDataFunction&) = delete;
  PrinterProviderInternalGetPrintDataFunction& operator=(
      const PrinterProviderInternalGetPrintDataFunction&) = delete;

 protected:
  ~PrinterProviderInternalGetPrintDataFunction() override;
  ExtensionFunction::ResponseAction Run() override;

 private:
  void OnBlob(const scoped_refptr<base::RefCountedMemory>& data,
              std::unique_ptr<content::BlobHandle> blob);
  DECLARE_EXTENSION_FUNCTION("printerProviderInternal.getPrintData",
                             PRINTERPROVIDERINTERNAL_GETPRINTDATA)
};

class PrinterProviderInternalReportUsbPrinterInfoFunction
    : public ExtensionFunction {
 public:
  PrinterProviderInternalReportUsbPrinterInfoFunction();

  PrinterProviderInternalReportUsbPrinterInfoFunction(
      const PrinterProviderInternalReportUsbPrinterInfoFunction&) = delete;
  PrinterProviderInternalReportUsbPrinterInfoFunction& operator=(
      const PrinterProviderInternalReportUsbPrinterInfoFunction&) = delete;

 protected:
  ~PrinterProviderInternalReportUsbPrinterInfoFunction() override;
  ExtensionFunction::ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printerProviderInternal.reportUsbPrinterInfo",
                             PRINTERPROVIDERINTERNAL_REPORTUSBPRINTERINFO)
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_INTERNAL_API_H_
