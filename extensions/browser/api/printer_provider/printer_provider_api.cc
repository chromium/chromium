// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/printer_provider/printer_provider_api.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/device_event_log/device_event_log.h"
#include "extensions/browser/api/printer_provider/printer_provider_internal_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_internal_api_observer.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/api/printer_provider.h"
#include "extensions/common/api/printer_provider_internal.h"
#include "extensions/common/api/usb.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"

namespace extensions {

namespace {

// The separator between extension id and the extension's internal printer id
// used when generating a printer id unique across extensions.
const char kPrinterIdSeparator = ':';

// Given an extension ID and an ID of a printer reported by the extension, it
// generates a ID for the printer unique across extensions (assuming that the
// printer id is unique in the extension's space).
std::string GeneratePrinterId(const ExtensionId& extension_id,
                              const std::string& internal_printer_id) {
  std::string result = extension_id;
  result.append(1, kPrinterIdSeparator);
  result.append(internal_printer_id);
  return result;
}

// Parses an ID created using |GeneratePrinterId| to it's components:
// the extension ID and the printer ID internal to the extension.
// Returns whenter the ID was succesfully parsed.
bool ParsePrinterId(const std::string& printer_id,
                    std::string* extension_id,
                    std::string* internal_printer_id) {
  size_t separator = printer_id.find_first_of(kPrinterIdSeparator);
  if (separator == std::string::npos)
    return false;
  *extension_id = printer_id.substr(0, separator);
  *internal_printer_id = printer_id.substr(separator + 1);
  return true;
}

void UpdatePrinterWithExtensionInfo(base::Value::Dict* printer,
                                    const Extension* extension) {
  std::string* internal_printer_id = printer->FindString("id");
  CHECK(internal_printer_id);
  printer->Set("id", GeneratePrinterId(extension->id(), *internal_printer_id));
  printer->Set("extensionId", extension->id());
  printer->Set("extensionName", extension->name());

  std::string* printer_name = printer->FindString("name");
  if (printer_name) {
    std::u16string u16_printer_name = base::UTF8ToUTF16(*printer_name);
    if (base::i18n::AdjustStringForLocaleDirection(&u16_printer_name))
      printer->Set("name", u16_printer_name);
  }

  std::string* printer_description = printer->FindString("description");
  if (printer_description) {
    std::u16string u16_printer_description =
        base::UTF8ToUTF16(*printer_description);
    if (base::i18n::AdjustStringForLocaleDirection(&u16_printer_description))
      printer->Set("description", u16_printer_description);
  }
}

// Holds information about a pending onGetPrintersRequested request;
// in particular, the list of extensions to which the event was dispatched but
// which haven't yet responded, and the |GetPrinters| callback associated with
// the event.
class GetPrintersRequest {
 public:
  explicit GetPrintersRequest(
      const PrinterProviderAPI::GetPrintersCallback& callback);
  ~GetPrintersRequest();

  // Adds an extension id to the list of the extensions that need to respond
  // to the event.
  void AddSource(const ExtensionId& extension_id);

  // Whether all extensions have responded to the event.
  bool IsDone() const;

  // Runs the callback for an extension and removes the extension from the
  // list of extensions that still have to respond to the event.
  void ReportForExtension(const ExtensionId& extension_id,
                          base::Value::List printers);

 private:
  // Callback reporting event result for an extension. Called once for each
  // extension.
  PrinterProviderAPI::GetPrintersCallback callback_;

  // The list of extensions that still have to respond to the event.
  std::set<std::string> extensions_;
};

// Keeps track of pending chrome.printerProvider.onGetPrintersRequested
// requests.
class PendingGetPrintersRequests {
 public:
  PendingGetPrintersRequests();
  PendingGetPrintersRequests(const PendingGetPrintersRequests&) = delete;
  PendingGetPrintersRequests& operator=(const PendingGetPrintersRequests&) =
      delete;
  ~PendingGetPrintersRequests();

  // Adds a new request to the set of pending requests. Returns the id
  // assigned to the request.
  int Add(const PrinterProviderAPI::GetPrintersCallback& callback);

  // Completes a request for an extension. It runs the request callback with
  // values reported by the extension.
  bool CompleteForExtension(const ExtensionId& extension_id,
                            int request_id,
                            base::Value::List result);

  // Runs callbacks for the extension for all requests that are waiting for a
  // response from the extension with the provided extension id. Callbacks are
  // called as if the extension reported empty set of printers.
  void FailAllForExtension(const ExtensionId& extension_id);

  // Adds an extension id to the list of the extensions that need to respond to
  // the event.
  bool AddSource(int request_id, const ExtensionId& extension_id);

 private:
  int last_request_id_;
  std::map<int, GetPrintersRequest> pending_requests_;
};

// Keeps track of pending chrome.printerProvider.onGetCapabilityRequested
// requests for an extension.
class PendingGetCapabilityRequests {
 public:
  static constexpr base::TimeDelta kGetCapabilityTimeout = base::Seconds(20);

  PendingGetCapabilityRequests();
  ~PendingGetCapabilityRequests();

  // Adds a new request to the set. Only information needed is the callback
  // associated with the request. Returns the id assigned to the request.
  int Add(PrinterProviderAPI::GetCapabilityCallback callback);

  // Completes the request with the provided request id. It runs the request
  // callback and removes the request from the set.
  void Complete(int request_id, base::Value::Dict result);

  // Runs all pending callbacks with empty capability value and clears the
  // set of pending requests.
  void FailAll();

 private:
  int last_request_id_;
  std::map<int, PrinterProviderAPI::GetCapabilityCallback> pending_requests_;
  base::WeakPtrFactory<PendingGetCapabilityRequests> weak_factory_{this};
};

constexpr base::TimeDelta PendingGetCapabilityRequests::kGetCapabilityTimeout;

// Keeps track of pending chrome.printerProvider.onPrintRequested requests
// for an extension.
class PendingPrintRequests {
 public:
  PendingPrintRequests();
  ~PendingPrintRequests();

  // Adds a new request to the set. Only information needed is the callback
  // associated with the request. Returns the id assigned to the request.
  int Add(PrinterProviderPrintJob job,
          PrinterProviderAPI::PrintCallback callback);

  // Gets print job associated with a request.
  const PrinterProviderPrintJob* GetPrintJob(int request_id) const;

  // Completes the request with the provided request id. It runs the request
  // callback and removes the request from the set.
  bool Complete(int request_id,
                api::printer_provider_internal::PrintError error);

  // Runs all pending callbacks with ERROR_FAILED and clears the set of
  // pending requests.
  void FailAll();

 private:
  struct PrintRequest {
    PrinterProviderAPI::PrintCallback callback;
    PrinterProviderPrintJob job;
  };

  int last_request_id_;
  std::map<int, PrintRequest> pending_requests_;
};

// Keeps track of pending chrome.printerProvider.onGetUsbPrinterInfoRequested
// requests for an extension.
class PendingUsbPrinterInfoRequests {
 public:
  PendingUsbPrinterInfoRequests();
  ~PendingUsbPrinterInfoRequests();

  // Adds a new request to the set. Only information needed is the callback
  // associated with the request. Returns the id assigned to the request.
  int Add(PrinterProviderAPI::GetPrinterInfoCallback callback);

  // Completes the request with the provided request id. It runs the request
  // callback and removes the request from the set.
  void Complete(int request_id, base::Value::Dict printer_info);

  // Runs all pending callbacks with empty capability value and clears the
  // set of pending requests.
  void FailAll();

 private:
  int last_request_id_ = 0;
  std::map<int, PrinterProviderAPI::GetPrinterInfoCallback> pending_requests_;
};

// Implements chrome.printerProvider API events.
class PrinterProviderAPIImpl : public PrinterProviderAPI,
                               public PrinterProviderInternalAPIObserver,
                               public ExtensionRegistryObserver {
 public:
  explicit PrinterProviderAPIImpl(content::BrowserContext* browser_context);
  PrinterProviderAPIImpl(const PrinterProviderAPIImpl&) = delete;
  PrinterProviderAPIImpl& operator=(const PrinterProviderAPIImpl&) = delete;
  ~PrinterProviderAPIImpl() override;

 private:
  // PrinterProviderAPI implementation:
  void DispatchGetPrintersRequested(
      const GetPrintersCallback& callback) override;
  void DispatchGetCapabilityRequested(const std::string& printer_id,
                                      GetCapabilityCallback callback) override;
  void DispatchPrintRequested(PrinterProviderPrintJob job,
                              PrintCallback callback) override;
  const PrinterProviderPrintJob* GetPrintJob(const Extension* extension,
                                             int request_id) const override;
  void DispatchGetUsbPrinterInfoRequested(
      const ExtensionId& extension_id,
      const device::mojom::UsbDeviceInfo& device,
      GetPrinterInfoCallback callback) override;

  // PrinterProviderInternalAPIObserver implementation:
  void OnGetPrintersResult(
      const Extension* extension,
      int request_id,
      const PrinterProviderInternalAPIObserver::PrinterInfoVector& result)
      override;
  void OnGetCapabilityResult(const Extension* extension,
                             int request_id,
                             base::Value::Dict result) override;
  void OnPrintResult(const Extension* extension,
                     int request_id,
                     api::printer_provider_internal::PrintError error) override;
  void OnGetUsbPrinterInfoResult(
      const Extension* extension,
      int request_id,
      const api::printer_provider::PrinterInfo* printer_info) override;

  // ExtensionRegistryObserver implementation:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Called before chrome.printerProvider.onGetPrintersRequested event is
  // dispatched to an extension. It returns whether the extension is interested
  // in the event. If the extension listens to the event, it's added to the set
  // of |request| sources. |request| is |GetPrintersRequest| object associated
  // with the event.
  bool WillRequestPrinters(
      int request_id,
      content::BrowserContext* browser_context,
      mojom::ContextType target_context,
      const Extension* extension,
      const base::Value::Dict* listener_filter,
      std::optional<base::Value::List>& event_args_out,
      mojom::EventFilteringInfoPtr& event_filtering_info_out);

  raw_ptr<content::BrowserContext> browser_context_;

  PendingGetPrintersRequests pending_get_printers_requests_;

  std::map<std::string, PendingPrintRequests> pending_print_requests_;

  std::map<std::string, PendingGetCapabilityRequests>
      pending_capability_requests_;

  std::map<std::string, PendingUsbPrinterInfoRequests>
      pending_usb_printer_info_requests_;

  base::ScopedObservation<PrinterProviderInternalAPI,
                          PrinterProviderInternalAPIObserver>
      internal_api_observation_{this};

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

GetPrintersRequest::GetPrintersRequest(
    const PrinterProviderAPI::GetPrintersCallback& callback)
    : callback_(callback) {
}

GetPrintersRequest::~GetPrintersRequest() {
}

void GetPrintersRequest::AddSource(const ExtensionId& extension_id) {
  extensions_.insert(extension_id);
}

bool GetPrintersRequest::IsDone() const {
  return extensions_.empty();
}

void GetPrintersRequest::ReportForExtension(const ExtensionId& extension_id,
                                            base::Value::List printers) {
  if (extensions_.erase(extension_id) > 0)
    callback_.Run(std::move(printers), IsDone());
}

PendingGetPrintersRequests::PendingGetPrintersRequests() : last_request_id_(0) {
}

PendingGetPrintersRequests::~PendingGetPrintersRequests() {
}

int PendingGetPrintersRequests::Add(
    const PrinterProviderAPI::GetPrintersCallback& callback) {
  pending_requests_.insert(
      std::make_pair(++last_request_id_, GetPrintersRequest(callback)));
  return last_request_id_;
}

bool PendingGetPrintersRequests::CompleteForExtension(
    const ExtensionId& extension_id,
    int request_id,
    base::Value::List result) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  it->second.ReportForExtension(extension_id, std::move(result));
  if (it->second.IsDone()) {
    pending_requests_.erase(it);
  }
  return true;
}

void PendingGetPrintersRequests::FailAllForExtension(
    const ExtensionId& extension_id) {
  auto it = pending_requests_.begin();
  while (it != pending_requests_.end()) {
    int request_id = it->first;
    // |it| may get deleted during |CompleteForExtension|, so progress it to the
    // next item before calling the method.
    ++it;
    CompleteForExtension(extension_id, request_id, base::Value::List());
  }
}

bool PendingGetPrintersRequests::AddSource(int request_id,
                                           const ExtensionId& extension_id) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  it->second.AddSource(extension_id);
  return true;
}

PendingGetCapabilityRequests::PendingGetCapabilityRequests()
    : last_request_id_(0) {
}

PendingGetCapabilityRequests::~PendingGetCapabilityRequests() {
}

int PendingGetCapabilityRequests::Add(
    PrinterProviderAPI::GetCapabilityCallback callback) {
  pending_requests_[++last_request_id_] = std::move(callback);
  // Abort the request after the timeout is exceeded.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PendingGetCapabilityRequests::Complete,
                     weak_factory_.GetWeakPtr(), last_request_id_,
                     base::Value::Dict()),
      kGetCapabilityTimeout);
  return last_request_id_;
}

void PendingGetCapabilityRequests::Complete(int request_id,
                                            base::Value::Dict response) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return;

  PrinterProviderAPI::GetCapabilityCallback callback = std::move(it->second);
  pending_requests_.erase(it);

  std::move(callback).Run(std::move(response));
}

void PendingGetCapabilityRequests::FailAll() {
  for (auto& request : pending_requests_)
    std::move(request.second).Run(base::Value::Dict());
  pending_requests_.clear();
}

PendingPrintRequests::PendingPrintRequests() : last_request_id_(0) {
}

PendingPrintRequests::~PendingPrintRequests() {
}

int PendingPrintRequests::Add(PrinterProviderPrintJob job,
                              PrinterProviderAPI::PrintCallback callback) {
  PrintRequest request;
  request.callback = std::move(callback);
  request.job = std::move(job);
  pending_requests_[++last_request_id_] = std::move(request);
  return last_request_id_;
}

bool PendingPrintRequests::Complete(
    int request_id,
    api::printer_provider_internal::PrintError error) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  PrinterProviderAPI::PrintCallback callback = std::move(it->second.callback);
  pending_requests_.erase(it);

  base::Value error_value;
  if (error != api::printer_provider_internal::PrintError::kOk) {
    const std::string error_str =
        error == api::printer_provider_internal::PrintError::kNone
            ? PrinterProviderAPI::GetDefaultPrintError()
            : api::printer_provider_internal::ToString(error);
    error_value = base::Value(error_str);
  }
  std::move(callback).Run(error_value);
  return true;
}

const PrinterProviderPrintJob* PendingPrintRequests::GetPrintJob(
    int request_id) const {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return nullptr;

  return &it->second.job;
}

void PendingPrintRequests::FailAll() {
  for (auto& request : pending_requests_) {
    std::move(request.second.callback)
        .Run(base::Value(PrinterProviderAPI::GetDefaultPrintError()));
  }
  pending_requests_.clear();
}

PendingUsbPrinterInfoRequests::PendingUsbPrinterInfoRequests() {
}

PendingUsbPrinterInfoRequests::~PendingUsbPrinterInfoRequests() {
}

int PendingUsbPrinterInfoRequests::Add(
    PrinterProviderAPI::GetPrinterInfoCallback callback) {
  pending_requests_[++last_request_id_] = std::move(callback);
  return last_request_id_;
}

void PendingUsbPrinterInfoRequests::Complete(int request_id,
                                             base::Value::Dict printer_info) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return;

  PrinterProviderAPI::GetPrinterInfoCallback callback = std::move(it->second);
  pending_requests_.erase(it);

  std::move(callback).Run(std::move(printer_info));
}

void PendingUsbPrinterInfoRequests::FailAll() {
  for (auto& request : pending_requests_) {
    std::move(request.second).Run(base::Value::Dict());
  }
  pending_requests_.clear();
}

PrinterProviderAPIImpl::PrinterProviderAPIImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  internal_api_observation_.Observe(
      PrinterProviderInternalAPI::GetFactoryInstance()->Get(browser_context));
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context));
}

PrinterProviderAPIImpl::~PrinterProviderAPIImpl() {
}

void PrinterProviderAPIImpl::DispatchGetPrintersRequested(
    const GetPrintersCallback& callback) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->HasEventListener(
          api::printer_provider::OnGetPrintersRequested::kEventName)) {
    callback.Run(base::Value::List(), /*done=*/true);
    return;
  }

  // |pending_get_printers_requests_| take ownership of |request| which gets
  // NULLed out. Save the pointer before passing it to the requests, as it will
  // be needed later on.
  int request_id = pending_get_printers_requests_.Add(callback);

  base::Value::List internal_args;
  // Request id is not part of the public API, but it will be massaged out in
  // custom bindings.
  internal_args.Append(request_id);

  auto event = std::make_unique<Event>(
      events::PRINTER_PROVIDER_ON_GET_PRINTERS_REQUESTED,
      api::printer_provider::OnGetPrintersRequested::kEventName,
      std::move(internal_args));
  // This callback is called synchronously during |BroadcastEvent|, so
  // Unretained is safe.
  event->will_dispatch_callback =
      base::BindRepeating(&PrinterProviderAPIImpl::WillRequestPrinters,
                          base::Unretained(this), request_id);

  event_router->BroadcastEvent(std::move(event));
}

void PrinterProviderAPIImpl::DispatchGetCapabilityRequested(
    const std::string& printer_id,
    GetCapabilityCallback callback) {
  ExtensionId extension_id;
  std::string internal_printer_id;
  if (!ParsePrinterId(printer_id, &extension_id, &internal_printer_id)) {
    std::move(callback).Run(base::Value::Dict());
    return;
  }

  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->ExtensionHasEventListener(
          extension_id,
          api::printer_provider::OnGetCapabilityRequested::kEventName)) {
    std::move(callback).Run(base::Value::Dict());
    return;
  }

  int request_id =
      pending_capability_requests_[extension_id].Add(std::move(callback));

  base::Value::List internal_args;
  // Request id is not part of the public API, but it will be massaged out in
  // custom bindings.
  internal_args.Append(request_id);
  internal_args.Append(internal_printer_id);

  std::unique_ptr<Event> event(
      new Event(events::PRINTER_PROVIDER_ON_GET_CAPABILITY_REQUESTED,
                api::printer_provider::OnGetCapabilityRequested::kEventName,
                std::move(internal_args)));

  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

void PrinterProviderAPIImpl::DispatchPrintRequested(PrinterProviderPrintJob job,
                                                    PrintCallback callback) {
  ExtensionId extension_id;
  std::string internal_printer_id;
  if (!ParsePrinterId(job.printer_id, &extension_id, &internal_printer_id)) {
    std::move(callback).Run(base::Value(GetDefaultPrintError()));
    return;
  }

  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->ExtensionHasEventListener(
          extension_id, api::printer_provider::OnPrintRequested::kEventName)) {
    std::move(callback).Run(base::Value(GetDefaultPrintError()));
    return;
  }

  api::printer_provider::PrintJob print_job;
  print_job.printer_id = internal_printer_id;

  if (auto ticket =
          api::printer_provider::PrintJob::Ticket::FromValue(job.ticket);
      !ticket) {
    std::move(callback).Run(base::Value(api::printer_provider::ToString(
        api::printer_provider::PrintError::kInvalidTicket)));
    return;
  } else {
    print_job.ticket = std::move(ticket).value();
  }

  print_job.content_type = job.content_type;
  print_job.title = base::UTF16ToUTF8(job.job_title);
  int request_id = pending_print_requests_[extension_id].Add(
      std::move(job), std::move(callback));

  base::Value::List internal_args;
  // Request id is not part of the public API and it will be massaged out in
  // custom bindings.
  internal_args.Append(request_id);
  internal_args.Append(print_job.ToValue());
  auto event = std::make_unique<Event>(
      events::PRINTER_PROVIDER_ON_PRINT_REQUESTED,
      api::printer_provider::OnPrintRequested::kEventName,
      std::move(internal_args));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

const PrinterProviderPrintJob* PrinterProviderAPIImpl::GetPrintJob(
    const Extension* extension,
    int request_id) const {
  auto it = pending_print_requests_.find(extension->id());
  if (it == pending_print_requests_.end())
    return nullptr;
  return it->second.GetPrintJob(request_id);
}

void PrinterProviderAPIImpl::DispatchGetUsbPrinterInfoRequested(
    const ExtensionId& extension_id,
    const device::mojom::UsbDeviceInfo& device,
    GetPrinterInfoCallback callback) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->ExtensionHasEventListener(
          extension_id,
          api::printer_provider::OnGetUsbPrinterInfoRequested::kEventName)) {
    std::move(callback).Run(base::Value::Dict());
    return;
  }

  int request_id =
      pending_usb_printer_info_requests_[extension_id].Add(std::move(callback));
  api::usb::Device api_device;
  UsbDeviceManager::Get(browser_context_)->GetApiDevice(device, &api_device);

  base::Value::List internal_args;
  // Request id is not part of the public API and it will be massaged out in
  // custom bindings.
  internal_args.Append(request_id);
  internal_args.Append(api_device.ToValue());
  auto event = std::make_unique<Event>(
      events::PRINTER_PROVIDER_ON_GET_USB_PRINTER_INFO_REQUESTED,
      api::printer_provider::OnGetUsbPrinterInfoRequested::kEventName,
      std::move(internal_args));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

void PrinterProviderAPIImpl::OnGetPrintersResult(
    const Extension* extension,
    int request_id,
    const PrinterProviderInternalAPIObserver::PrinterInfoVector& result) {
  base::Value::List printer_list;

  // Update some printer description properties to better identify the extension
  // managing the printer.
  for (const api::printer_provider::PrinterInfo& p : result) {
    base::Value::Dict printer(p.ToValue());
    UpdatePrinterWithExtensionInfo(&printer, extension);
    printer_list.Append(std::move(printer));
  }

  PRINTER_LOG(DEBUG) << "Notifying extensionID=" << extension->id()
                     << " of OnGetPrinters request=" << request_id
                     << " completed with printers=" << printer_list;

  pending_get_printers_requests_.CompleteForExtension(
      extension->id(), request_id, std::move(printer_list));
}

void PrinterProviderAPIImpl::OnGetCapabilityResult(const Extension* extension,
                                                   int request_id,
                                                   base::Value::Dict result) {
  PRINTER_LOG(DEBUG) << "Notifying extensionID=" << extension->id()
                     << " that OnGetCapabilility request id=" << request_id
                     << " completed with capabilities=" << result;
  pending_capability_requests_[extension->id()].Complete(request_id,
                                                         std::move(result));
}

void PrinterProviderAPIImpl::OnPrintResult(
    const Extension* extension,
    int request_id,
    api::printer_provider_internal::PrintError error) {
  PRINTER_LOG(DEBUG) << "Notifying extensionID=" << extension->id()
                     << " that OnPrint request id=" << request_id
                     << " has completed with status="
                     << api::printer_provider_internal::ToString(error);
  pending_print_requests_[extension->id()].Complete(request_id, error);
}

void PrinterProviderAPIImpl::OnGetUsbPrinterInfoResult(
    const Extension* extension,
    int request_id,
    const api::printer_provider::PrinterInfo* result) {
  if (result) {
    base::Value::Dict printer(result->ToValue());
    UpdatePrinterWithExtensionInfo(&printer, extension);
    PRINTER_LOG(DEBUG) << "Notifying extensionID=" << extension->id()
                       << " from request=" << request_id << " that "
                       << result->name << " is a USB connected printer";
    pending_usb_printer_info_requests_[extension->id()].Complete(
        request_id, std::move(printer));
  } else {
    PRINTER_LOG(DEBUG) << "Notifying extensionID=" << extension->id()
                       << " from request=" << request_id
                       << " that there are no USB connected printers";
    pending_usb_printer_info_requests_[extension->id()].Complete(
        request_id, base::Value::Dict());
  }
}

void PrinterProviderAPIImpl::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  PRINTER_LOG(DEBUG) << "Unloading Extension: Name=" << extension->name()
                     << " Version=" << extension->VersionString()
                     << " extensionID=" << extension->id()
                     << " for reason=" << base::ToString(reason);
  pending_get_printers_requests_.FailAllForExtension(extension->id());

  auto print_it = pending_print_requests_.find(extension->id());
  if (print_it != pending_print_requests_.end()) {
    print_it->second.FailAll();
    pending_print_requests_.erase(print_it);
  }

  auto capability_it = pending_capability_requests_.find(extension->id());
  if (capability_it != pending_capability_requests_.end()) {
    capability_it->second.FailAll();
    pending_capability_requests_.erase(capability_it);
  }

  auto usb_it = pending_usb_printer_info_requests_.find(extension->id());
  if (usb_it != pending_usb_printer_info_requests_.end()) {
    usb_it->second.FailAll();
    pending_usb_printer_info_requests_.erase(usb_it);
  }
}

bool PrinterProviderAPIImpl::WillRequestPrinters(
    int request_id,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out) {
  if (!extension)
    return false;
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router->ExtensionHasEventListener(
          extension->id(),
          api::printer_provider::OnGetPrintersRequested::kEventName)) {
    return false;
  }

  return pending_get_printers_requests_.AddSource(request_id, extension->id());
}

}  // namespace

// static
PrinterProviderAPI* PrinterProviderAPI::Create(
    content::BrowserContext* context) {
  return new PrinterProviderAPIImpl(context);
}

// static
std::string PrinterProviderAPI::GetDefaultPrintError() {
  return api::printer_provider_internal::ToString(
      api::printer_provider_internal::PrintError::kFailed);
}

}  // namespace extensions
