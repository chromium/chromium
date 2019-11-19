// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/printer_provider_internal/printer_provider_internal_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
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
#include "extensions/browser/blob_holder.h"
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
    content::BrowserContext* browser_context) {
}

PrinterProviderInternalAPI::~PrinterProviderInternalAPI() {
}

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
    const base::DictionaryValue& capability) {
  for (auto& observer : observers_)
    observer.OnGetCapabilityResult(extension, request_id, capability);
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
    PrinterProviderInternalReportPrintResultFunction() {
}

PrinterProviderInternalReportPrintResultFunction::
    ~PrinterProviderInternalReportPrintResultFunction() {
}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportPrintResultFunction::Run() {
  std::unique_ptr<internal_api::ReportPrintResult::Params> params(
      internal_api::ReportPrintResult::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrinterProviderInternalAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->NotifyPrintResult(extension(), params->request_id, params->error);
  return RespondNow(NoArguments());
}

PrinterProviderInternalReportPrinterCapabilityFunction::
    PrinterProviderInternalReportPrinterCapabilityFunction() {
}

PrinterProviderInternalReportPrinterCapabilityFunction::
    ~PrinterProviderInternalReportPrinterCapabilityFunction() {
}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportPrinterCapabilityFunction::Run() {
  std::unique_ptr<internal_api::ReportPrinterCapability::Params> params(
      internal_api::ReportPrinterCapability::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->capability) {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetCapabilityResult(extension(), params->request_id,
                                    params->capability->additional_properties);
  } else {
    PrinterProviderInternalAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->NotifyGetCapabilityResult(extension(), params->request_id,
                                    base::DictionaryValue());
  }
  return RespondNow(NoArguments());
}

PrinterProviderInternalReportPrintersFunction::
    PrinterProviderInternalReportPrintersFunction() {
}

PrinterProviderInternalReportPrintersFunction::
    ~PrinterProviderInternalReportPrintersFunction() {
}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportPrintersFunction::Run() {
  std::unique_ptr<internal_api::ReportPrinters::Params> params(
      internal_api::ReportPrinters::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::ListValue printers;
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
    PrinterProviderInternalGetPrintDataFunction() {
}

PrinterProviderInternalGetPrintDataFunction::
    ~PrinterProviderInternalGetPrintDataFunction() {
}

ExtensionFunction::ResponseAction
PrinterProviderInternalGetPrintDataFunction::Run() {
  std::unique_ptr<internal_api::GetPrintData::Params> params(
      internal_api::GetPrintData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const PrinterProviderPrintJob* job =
      PrinterProviderAPIFactory::GetInstance()
          ->GetForBrowserContext(browser_context())
          ->GetPrintJob(extension(), params->request_id);
  if (!job)
    return RespondNow(Error("Print request not found."));

  if (!job->document_bytes)
    return RespondNow(Error("Job data not set"));

  // |job->document_bytes| are passed to the callback to make sure the ref
  // counted memory does not go away before the memory backed blob is created.
  content::BrowserContext::CreateMemoryBackedBlob(
      browser_context(),
      base::make_span(job->document_bytes->front(),
                      job->document_bytes->size()),
      "",
      base::BindOnce(&PrinterProviderInternalGetPrintDataFunction::OnBlob, this,
                     job->content_type, job->document_bytes->size(),
                     job->document_bytes));
  return RespondLater();
}

void PrinterProviderInternalGetPrintDataFunction::OnBlob(
    const std::string& type,
    int size,
    const scoped_refptr<base::RefCountedMemory>& data,
    std::unique_ptr<content::BlobHandle> blob) {
  if (!blob) {
    Respond(Error("Unable to create the blob."));
    return;
  }

  internal_api::BlobInfo info;
  info.blob_uuid = blob->GetUUID();
  info.type = type;
  info.size = size;

  std::vector<std::string> uuids;
  uuids.push_back(blob->GetUUID());

  extensions::BlobHolder* holder =
      extensions::BlobHolder::FromRenderProcessHost(
          render_frame_host()->GetProcess());
  holder->HoldBlobReference(std::move(blob));

  SetTransferredBlobUUIDs(uuids);
  Respond(ArgumentList(internal_api::GetPrintData::Results::Create(info)));
}

PrinterProviderInternalReportUsbPrinterInfoFunction::
    PrinterProviderInternalReportUsbPrinterInfoFunction() {
}

PrinterProviderInternalReportUsbPrinterInfoFunction::
    ~PrinterProviderInternalReportUsbPrinterInfoFunction() {
}

ExtensionFunction::ResponseAction
PrinterProviderInternalReportUsbPrinterInfoFunction::Run() {
  std::unique_ptr<internal_api::ReportUsbPrinterInfo::Params> params(
      internal_api::ReportUsbPrinterInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrinterProviderInternalAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->NotifyGetUsbPrinterInfoResult(extension(), params->request_id,
                                      params->printer_info.get());
  return RespondNow(NoArguments());
}

}  // namespace extensions
