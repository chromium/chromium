// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "printing/backend/print_backend_cups.h"

#include <cups/cups.h>
#include <cups/ppd.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>

#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "printing/backend/cups_helper.h"
#include "printing/backend/cups_weak_functions.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/print_backend_utils.h"
#include "printing/mojom/print.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#include "base/feature_list.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/print_backend_cups_ipp.h"
#include "printing/printing_features.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

namespace printing {

namespace {

struct CupsDestsData {
  int num_dests;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #reinterpret-cast-trivial-type, #addr-of
  RAW_PTR_EXCLUSION cups_dest_t* dests;
};

int CaptureCupsDestCallback(void* data, unsigned flags, cups_dest_t* dest) {
  CupsDestsData* dests_data = reinterpret_cast<CupsDestsData*>(data);
  if (flags & CUPS_DEST_FLAGS_REMOVED) {
    dests_data->num_dests = cupsRemoveDest(
        dest->name, dest->instance, dests_data->num_dests, &dests_data->dests);
  } else {
    dests_data->num_dests =
        cupsCopyDest(dest, dests_data->num_dests, &dests_data->dests);
  }
  return 1;  // Keep going.
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
// This may be removed when Amazon Linux 2 reaches EOL (30 Jun 2025).
bool AreNewerCupsFunctionsAvailable() {
  return cupsFindDestDefault && cupsFindDestSupported && cupsUserAgent &&
         ippValidateAttributes;
}
#endif

}  // namespace

PrintBackendCUPS::PrintBackendCUPS(const GURL& print_server_url,
                                   http_encryption_t encryption,
                                   bool blocking,
                                   const std::string& locale)
    : locale_(locale),
      print_server_url_(print_server_url),
      cups_encryption_(encryption),
      blocking_(blocking) {}

PrintBackendCUPS::~PrintBackendCUPS() = default;

// static
mojom::ResultCode PrintBackendCUPS::PrinterBasicInfoFromCUPS(
    const cups_dest_t& printer,
    PrinterBasicInfo* printer_info) {
  const char* type_str =
      cupsGetOption(kCUPSOptPrinterType, printer.num_options, printer.options);
  if (type_str) {
    cups_ptype_t type;
    if (base::StringToUint(type_str, &type)) {
      if (type & kDestinationsFilterMask)
        return mojom::ResultCode::kFailed;
    }
  }

  printer_info->printer_name = printer.name;
  printer_info->is_default = printer.is_default;

  const char* info_option =
      cupsGetOption(kCUPSOptPrinterInfo, printer.num_options, printer.options);

  const char* state =
      cupsGetOption(kCUPSOptPrinterState, printer.num_options, printer.options);
  if (state)
    base::StringToInt(state, &printer_info->printer_status);

  const char* drv_info = cupsGetOption(kCUPSOptPrinterMakeAndModel,
                                       printer.num_options, printer.options);
  if (drv_info)
    printer_info->options[kDriverInfoTagName] = drv_info;

  // Store printer options.
  for (int opt_index = 0; opt_index < printer.num_options; ++opt_index) {
    printer_info->options[printer.options[opt_index].name] =
        printer.options[opt_index].value;
  }
  std::string_view info =
      info_option ? std::string_view(info_option) : std::string_view();
  printer_info->display_name = GetDisplayName(printer_info->printer_name, info);
  printer_info->printer_description = GetPrinterDescription(
      drv_info ? std::string_view(drv_info) : std::string_view(), info);
  return mojom::ResultCode::kSuccess;
}

// static
std::string PrintBackendCUPS::PrinterDriverInfoFromCUPS(
    const cups_dest_t& printer) {
  const char* info =
      cupsGetOption(kDriverNameTagName, printer.num_options, printer.options);
  return info ? info : std::string();
}

mojom::ResultCode PrintBackendCUPS::EnumeratePrinters(
    PrinterList& printer_list) {
  DCHECK(printer_list.empty());

  // If possible prefer to use cupsEnumDests() over GetDests(), because the
  // latter has been found to filter out some destination values if a device
  // reports multiple times (crbug.com/1209175), which can lead to destinations
  // not showing as available.  Using cupsEnumDests() allows us to do our own
  // filtering should any duplicates occur.
  CupsDestsData dests_data = {0, nullptr};
  ipp_status_t last_error = IPP_STATUS_OK;
  if (print_server_url_.is_empty()) {
    VLOG(1) << "CUPS: using cupsEnumDests to enumerate printers";
    if (!cupsEnumDests(CUPS_DEST_FLAGS_NONE, kCupsTimeoutMs,
                       /*cancel=*/nullptr,
                       /*type=*/CUPS_PRINTER_LOCAL, kDestinationsFilterMask,
                       CaptureCupsDestCallback, &dests_data)) {
      // Free any allocations and reset data, and then fall through to common
      // error handling below.
      last_error = cupsLastError();
      cupsFreeDests(dests_data.num_dests, dests_data.dests);
      dests_data.num_dests = 0;
      dests_data.dests = nullptr;
    }
  } else {
    VLOG(1) << "CUPS: using cupsGetDests2 to enumerate printers";
    dests_data.num_dests = GetDests(&dests_data.dests);
    if (!dests_data.num_dests)
      last_error = cupsLastError();
  }

  DCHECK_GE(dests_data.num_dests, 0);
  if (!dests_data.num_dests) {
    // No destinations could mean the operation failed or that there are simply
    // no printer drivers installed.  Rely upon CUPS error code to distinguish
    // between these.
    DCHECK(!dests_data.dests);
    if (last_error != IPP_STATUS_ERROR_NOT_FOUND) {
      VLOG(1) << "CUPS: Error getting printers from CUPS server"
              << ", server: " << print_server_url_
              << ", error: " << static_cast<int>(last_error) << " - "
              << cupsLastErrorString();
      return mojom::ResultCode::kFailed;
    }
    VLOG(1) << "CUPS: No printers found for CUPS server: " << print_server_url_;
    return mojom::ResultCode::kSuccess;
  }

  for (int printer_index = 0; printer_index < dests_data.num_dests;
       ++printer_index) {
    const cups_dest_t& printer = dests_data.dests[printer_index];

    PrinterBasicInfo printer_info;
    if (PrinterBasicInfoFromCUPS(printer, &printer_info) ==
        mojom::ResultCode::kSuccess) {
      printer_list.push_back(printer_info);
    }
  }

  cupsFreeDests(dests_data.num_dests, dests_data.dests);

  VLOG(1) << "CUPS: Enumerated printers, server: " << print_server_url_
          << ", # of printers: " << printer_list.size();
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendCUPS::GetDefaultPrinterName(
    std::string& default_printer) {
  // Not using cupsGetDefault() because it lies about the default printer.
  cups_dest_t* dests;
  int num_dests = GetDests(&dests);
  cups_dest_t* dest = cupsGetDest(nullptr, nullptr, num_dests, dests);
  mojom::ResultCode result = mojom::ResultCode::kSuccess;
  if (dest) {
    default_printer = std::string(dest->name);
  } else if (cupsLastError() <= IPP_OK_EVENTS_COMPLETE) {
    // No default printer found.
    default_printer.clear();
  } else {
    LOG(ERROR) << "CUPS: Error getting default printer: "
               << cupsLastErrorString();
    result = mojom::ResultCode::kFailed;
  }

  cupsFreeDests(num_dests, dests);
  return result;
}

mojom::ResultCode PrintBackendCUPS::GetPrinterBasicInfo(
    const std::string& printer_name,
    PrinterBasicInfo* printer_info) {
  ScopedDestination dest = GetNamedDest(printer_name);
  if (!dest)
    return mojom::ResultCode::kFailed;

  DCHECK_EQ(printer_name, dest->name);
  return PrinterBasicInfoFromCUPS(*dest, printer_info);
}

mojom::ResultCode PrintBackendCUPS::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  if (!IsValidPrinter(printer_name))
    return mojom::ResultCode::kFailed;

  std::string printer_capabilities = GetPrinterCapabilities(printer_name);
  if (printer_capabilities.empty()) {
    return mojom::ResultCode::kFailed;
  }

  ScopedDestination dest = GetNamedDest(printer_name);
  return ParsePpdCapabilities(dest.get(), locale_, printer_capabilities,
                              printer_info)
             ? mojom::ResultCode::kSuccess
             : mojom::ResultCode::kFailed;
}

std::string PrintBackendCUPS::GetPrinterCapabilities(
    const std::string& printer_name) {
  VLOG(1) << "CUPS: Getting caps and defaults, printer name: " << printer_name;

  base::FilePath ppd_path(GetPPD(printer_name.c_str()));
  // In some cases CUPS failed to get ppd file.
  if (ppd_path.empty()) {
    LOG(ERROR) << "CUPS: Failed to get PPD, printer name: " << printer_name;
    return std::string();
  }

  std::string content;
  if (!base::ReadFileToString(ppd_path, &content)) {
    content.clear();
  }

  base::DeleteFile(ppd_path);
  return content;
}

std::vector<std::string> PrintBackendCUPS::GetPrinterDriverInfo(
    const std::string& printer_name) {
  std::vector<std::string> result;

  ScopedDestination dest = GetNamedDest(printer_name);
  if (dest) {
    DCHECK_EQ(printer_name, dest->name);
    result.emplace_back(PrinterDriverInfoFromCUPS(*dest));
  }

  return result;
}

bool PrintBackendCUPS::IsValidPrinter(const std::string& printer_name) {
  return !!GetNamedDest(printer_name);
}

#if !BUILDFLAG(IS_CHROMEOS)
scoped_refptr<PrintBackend> PrintBackend::CreateInstanceImpl(
    const std::string& locale) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  if (AreNewerCupsFunctionsAvailable() &&
      base::FeatureList::IsEnabled(features::kCupsIppPrintingBackend)) {
    return base::MakeRefCounted<PrintBackendCupsIpp>(CupsConnection::Create());
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  return base::MakeRefCounted<PrintBackendCUPS>(
      GURL(), HTTP_ENCRYPT_NEVER, /*cups_blocking=*/false, locale);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

int PrintBackendCUPS::GetDests(cups_dest_t** dests) {
  // Default to the local print server (CUPS scheduler)
  if (print_server_url_.is_empty())
    return cupsGetDests2(CUPS_HTTP_DEFAULT, dests);

  HttpConnectionCUPS http(print_server_url_, cups_encryption_, blocking_);

  // This call must be made in the same scope as `http` because its destructor
  // closes the connection.
  return cupsGetDests2(http.http(), dests);
}

base::FilePath PrintBackendCUPS::GetPPD(const char* name) {
  // cupsGetPPD returns a filename stored in a static buffer in CUPS.
  // Protect this code with lock.
  static base::NoDestructor<base::Lock> ppd_lock;
  base::AutoLock ppd_autolock(*ppd_lock);
  base::FilePath ppd_path;
  const char* ppd_file_path = nullptr;
  if (print_server_url_.is_empty()) {  // Use default (local) print server.
    ppd_file_path = cupsGetPPD(name);
    if (ppd_file_path)
      ppd_path = base::FilePath(ppd_file_path);
  } else {
    // cupsGetPPD2 gets stuck sometimes in an infinite time due to network
    // configuration/issues. To prevent that, use non-blocking http connection
    // here.
    // Note: After looking at CUPS sources, it looks like non-blocking
    // connection will timeout after 10 seconds of no data period. And it will
    // return the same way as if data was completely and successfully
    // downloaded.
    HttpConnectionCUPS http(print_server_url_, cups_encryption_, blocking_);
    ppd_file_path = cupsGetPPD2(http.http(), name);
    // Check if the get full PPD, since non-blocking call may simply return
    // normally after timeout expired.
    if (ppd_file_path) {
      // There is no reliable way right now to detect full and complete PPD
      // get downloaded. If we reach http timeout, it may simply return
      // downloaded part as a full response. It might be good enough to check
      // http->data_remaining or http->_data_remaining, unfortunately http_t
      // is an internal structure and fields are not exposed in CUPS headers.
      // httpGetLength or httpGetLength2 returning the full content size.
      // Comparing file size against that content length might be unreliable
      // since some http reponses are encoded and content_length > file size.
      // Let's just check for the obvious CUPS and http errors here.
      ppd_path = base::FilePath(ppd_file_path);
      ipp_status_t error_code = cupsLastError();
      int http_error = httpError(http.http());
      if (error_code > IPP_OK_EVENTS_COMPLETE || http_error != 0) {
        LOG(ERROR) << "Error downloading PPD file, name: " << name
                   << ", CUPS error: " << static_cast<int>(error_code)
                   << ", HTTP error: " << http_error;
        base::DeleteFile(ppd_path);
        ppd_path.clear();
      }
    }
  }
  return ppd_path;
}

ScopedDestination PrintBackendCUPS::GetNamedDest(
    const std::string& printer_name) {
  cups_dest_t* dest;
  if (print_server_url_.is_empty()) {
    // Use default (local) print server.
    dest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, printer_name.c_str(), nullptr);
  } else {
    HttpConnectionCUPS http(print_server_url_, cups_encryption_, blocking_);
    dest = cupsGetNamedDest(http.http(), printer_name.c_str(), nullptr);
  }
  return ScopedDestination(dest);
}

}  // namespace printing
