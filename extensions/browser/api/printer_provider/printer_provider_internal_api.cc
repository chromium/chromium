// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/printer_provider/printer_provider_internal_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/observer_list.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "extensions/common/api/printer_provider.h"
#include "extensions/common/api/printer_provider_internal.h"

namespace internal_api = extensions::api::printer_provider_internal;

namespace extensions {

namespace {

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<PrinterProviderInternalAPI>>::DestructorAtExit
    g_api_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
BrowserContextKeyedAPIFactory<PrinterProviderInternalAPI>*
PrinterProviderInternalAPI::GetFactoryInstance() {
  return g_api_factory.Pointer();
}

PrinterProviderInternalAPI::PrinterProviderInternalAPI(
    content::BrowserContext* browser_context) {}

PrinterProviderInternalAPI::~PrinterProviderInternalAPI() = default;

void PrinterProviderInternalAPI::AddObserver(
    PrinterProviderInternalAPIObserver* observer) {
  observers_.AddObserver(observer);
}

void PrinterProviderInternalAPI::RemoveObserver(
    PrinterProviderInternalAPIObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PrinterProviderInternalAPI::NotifyGetPrintersResult(
    const Extension* extension,
    int request_id,
    const PrinterProviderInternalAPIObserver::PrinterInfoVector& printers) {
  for (auto& observer : observers_)
    observer.OnGetPrintersResult(extension, request_id, printers);
}

void PrinterProviderInternalAPI::NotifyGetCapabilityResult(
    const Extension* extension,
    int request_id,
    const base::Value::Dict& capability) {
  for (auto& observer : observers_)
    observer.OnGetCapabilityResult(extension, request_id, capability.Clone());
}

void PrinterProviderInternalAPI::NotifyPrintResult(
    const Extension* extension,
    int request_id,
    api::printer_provider_internal::PrintError error) {
  for (auto& observer : observers_)
    observer.OnPrintResult(extension, request_id, error);
}

void PrinterProviderInternalAPI::NotifyGetUsbPrinterInfoResult(
    const Extension* extension,
    int request_id,
    const api::printer_provider::PrinterInfo* printer_info) {
  for (auto& observer : observers_)
    observer.OnGetUsbPrinterInfoResult(extension, request_id, printer_info);
}

PrinterProviderInternalReportPrintResultFunction::
    PrinterProviderInternalReportPrintResultFunction() {}

PrinterProviderInternalReportPrintResultFunction::
    ~PrinterProviderInternalReportPrintResultFunction() {}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportPrintResultFunction::Run() {
  std::optional<internal_api::ReportPrintResult::Params> params =
      internal_api::ReportPrintResult::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  PrinterProviderInternalAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->NotifyPrintResult(extension(), params->request_id, params->error);
  return RespondNow(NoArguments());
}

PrinterProviderInternalReportPrinterCapabilityFunction::
    PrinterProviderInternalReportPrinterCapabilityFunction() {}

PrinterProviderInternalReportPrinterCapabilityFunction::
    ~PrinterProviderInternalReportPrinterCapabilityFunction() {}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportPrinterCapabilityFunction::Run() {
  std::optional<internal_api::ReportPrinterCapability::Params> params =
      internal_api::ReportPrinterCapability::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->capability) {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetCapabilityResult(extension(), params->request_id,
                                    params->capability->additional_properties);
  } else {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetCapabilityResult(extension(), params->request_id,
                                    base::Value::Dict());
  }
  return RespondNow(NoArguments());
}

PrinterProviderInternalReportPrintersFunction::
    PrinterProviderInternalReportPrintersFunction() {}

PrinterProviderInternalReportPrintersFunction::
    ~PrinterProviderInternalReportPrintersFunction() {}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportPrintersFunction::Run() {
  std::optional<internal_api::ReportPrinters::Params> params =
      internal_api::ReportPrinters::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->printers) {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetPrintersResult(extension(), params->request_id,
                                  *params->printers);
  } else {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetPrintersResult(
            extension(), params->request_id,
            PrinterProviderInternalAPIObserver::PrinterInfoVector());
  }
  return RespondNow(NoArguments());
}

PrinterProviderInternalGetPrintDataFunction::
    PrinterProviderInternalGetPrintDataFunction() {}

PrinterProviderInternalGetPrintDataFunction::
    ~PrinterProviderInternalGetPrintDataFunction() {}

ExtensionFunction::ResponseAction
PrinterProviderInternalGetPrintDataFunction::Run() {
  std::optional<internal_api::GetPrintData::Params> params =
      internal_api::GetPrintData::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const PrinterProviderPrintJob* job =
      PrinterProviderAPIFactory::GetInstance()
          ->GetForBrowserContext(browser_context())
          ->GetPrintJob(extension(), params->request_id);
  if (!job) {
    return RespondNow(Error("Print request not found."));
  }

  if (!job->document_bytes) {
    return RespondNow(Error("Job data not set"));
  }

  // |job->document_bytes| are passed to the callback to make sure the ref
  // counted memory does not go away before the memory backed blob is created.
  browser_context()->CreateMemoryBackedBlob(
      base::span(*job->document_bytes), job->content_type,
      base::BindOnce(&PrinterProviderInternalGetPrintDataFunction::OnBlob, this,
                     job->document_bytes));
  return RespondLater();
}

void PrinterProviderInternalGetPrintDataFunction::OnBlob(
    const scoped_refptr<base::RefCountedMemory>& data,
    std::unique_ptr<content::BlobHandle> blob) {
  if (!blob) {
    Respond(Error("Unable to create the blob."));
    return;
  }

  std::vector<blink::mojom::SerializedBlobPtr> blobs;
  blobs.push_back(blob->Serialize());

  SetTransferredBlobs(std::move(blobs));
  Respond(NoArguments());
}

PrinterProviderInternalReportUsbPrinterInfoFunction::
    PrinterProviderInternalReportUsbPrinterInfoFunction() {}

PrinterProviderInternalReportUsbPrinterInfoFunction::
    ~PrinterProviderInternalReportUsbPrinterInfoFunction() {}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportUsbPrinterInfoFunction::Run() {
  std::optional<internal_api::ReportUsbPrinterInfo::Params> params =
      internal_api::ReportUsbPrinterInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  PrinterProviderInternalAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->NotifyGetUsbPrinterInfoResult(
          extension(), params->request_id,
          base::OptionalToPtr(params->printer_info));
  return RespondNow(NoArguments());
}

}  // namespace extensions
