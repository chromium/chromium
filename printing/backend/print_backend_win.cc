// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include <objidl.h>
#include <stddef.h>
#include <winspool.h>
#include <wrl/client.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_hglobal.h"
#include "base/win/windows_types.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_info_win.h"
#include "printing/backend/win_helper.h"
#include "printing/mojom/print.mojom.h"

namespace printing {

namespace {

// Wrapper class to close provider automatically.
class ScopedProvider {
 public:
  explicit ScopedProvider(HPTPROVIDER provider) : provider_(provider) {}

  // Once the object is destroyed, it automatically closes the provider by
  // calling the XPSModule API.
  ~ScopedProvider() {
    if (provider_)
      XPSModule::CloseProvider(provider_);
  }

 private:
  HPTPROVIDER provider_;
};

// `GetResultCodeFromSystemErrorCode()` is only ever invoked when something has
// gone wrong while interacting with the OS printing system.  If the cause of
// the failure was not of the type to register and be and available from
// `GetLastError()` then we should just use the general error result.
mojom::ResultCode GetResultCodeFromSystemErrorCode(
    logging::SystemErrorCode system_code) {
  if (system_code == ERROR_ACCESS_DENIED)
    return mojom::ResultCode::kAccessDenied;
  return mojom::ResultCode::kFailed;
}

ScopedPrinterHandle GetPrinterHandle(const std::string& printer_name) {
  ScopedPrinterHandle handle;
  handle.OpenPrinterWithName(base::UTF8ToWide(printer_name).c_str());
  return handle;
}

HRESULT StreamOnHGlobalToString(IStream* stream, std::string* out) {
  DCHECK(stream);
  DCHECK(out);
  HGLOBAL hdata = nullptr;
  HRESULT hr = GetHGlobalFromStream(stream, &hdata);
  if (SUCCEEDED(hr)) {
    DCHECK(hdata);
    base::win::ScopedHGlobal<char*> locked_data(hdata);
    out->assign(locked_data.release(), locked_data.Size());
  }
  return hr;
}

template <class T>
void GetDeviceCapabilityArray(const wchar_t* printer,
                              const wchar_t* port,
                              WORD id,
                              std::vector<T>* result) {
  int count = DeviceCapabilities(printer, port, id, nullptr, nullptr);
  if (count <= 0)
    return;

  std::vector<T> tmp;
  tmp.resize(count * 2);
  count = DeviceCapabilities(printer, port, id,
                             reinterpret_cast<LPTSTR>(tmp.data()), nullptr);
  if (count <= 0)
    return;

  CHECK_LE(static_cast<size_t>(count), tmp.size());
  tmp.resize(count);
  result->swap(tmp);
}

void LoadPaper(const wchar_t* printer,
               const wchar_t* port,
               const DEVMODE* devmode,
               PrinterSemanticCapsAndDefaults* caps) {
  static const size_t kToUm = 100;  // Windows uses 0.1mm.
  static const size_t kMaxPaperName = 64;

  struct PaperName {
    wchar_t chars[kMaxPaperName];
  };

  DCHECK_EQ(sizeof(PaperName), sizeof(wchar_t) * kMaxPaperName);

  // Paper
  std::vector<PaperName> names;
  GetDeviceCapabilityArray(printer, port, DC_PAPERNAMES, &names);

  std::vector<POINT> sizes;
  GetDeviceCapabilityArray(printer, port, DC_PAPERSIZE, &sizes);

  std::vector<WORD> ids;
  GetDeviceCapabilityArray(printer, port, DC_PAPERS, &ids);

  DCHECK_EQ(ids.size(), sizes.size());
  DCHECK_EQ(names.size(), sizes.size());

  if (ids.size() != sizes.size())
    ids.clear();
  if (names.size() != sizes.size())
    names.clear();

  for (size_t i = 0; i < sizes.size(); ++i) {
    PrinterSemanticCapsAndDefaults::Paper paper;
    paper.size_um.SetSize(sizes[i].x * kToUm, sizes[i].y * kToUm);
    if (!names.empty()) {
      const wchar_t* name_start = names[i].chars;
      std::wstring tmp_name(name_start, kMaxPaperName);
      // Trim trailing zeros.
      tmp_name = tmp_name.c_str();
      paper.display_name = base::WideToUTF8(tmp_name);
    }
    if (!ids.empty())
      paper.vendor_id = base::NumberToString(ids[i]);
    caps->papers.push_back(paper);
  }

  if (!devmode)
    return;

  // Copy paper with the same ID as default paper.
  if (devmode->dmFields & DM_PAPERSIZE) {
    for (size_t i = 0; i < ids.size(); ++i) {
      if (ids[i] == devmode->dmPaperSize) {
        DCHECK_EQ(ids.size(), caps->papers.size());
        caps->default_paper = caps->papers[i];
        break;
      }
    }
  }

  gfx::Size default_size;
  if (devmode->dmFields & DM_PAPERWIDTH)
    default_size.set_width(devmode->dmPaperWidth * kToUm);
  if (devmode->dmFields & DM_PAPERLENGTH)
    default_size.set_height(devmode->dmPaperLength * kToUm);

  if (!default_size.IsEmpty()) {
    // Reset default paper if `dmPaperWidth` or `dmPaperLength` does not
    // match default paper set by.
    if (default_size != caps->default_paper.size_um)
      caps->default_paper = PrinterSemanticCapsAndDefaults::Paper();
    caps->default_paper.size_um = default_size;
  }
}

void LoadDpi(const wchar_t* printer,
             const wchar_t* port,
             const DEVMODE* devmode,
             PrinterSemanticCapsAndDefaults* caps) {
  std::vector<POINT> dpis;
  GetDeviceCapabilityArray(printer, port, DC_ENUMRESOLUTIONS, &dpis);

  for (size_t i = 0; i < dpis.size(); ++i)
    caps->dpis.push_back(gfx::Size(dpis[i].x, dpis[i].y));

  if (!devmode)
    return;

  if ((devmode->dmFields & DM_PRINTQUALITY) && devmode->dmPrintQuality > 0) {
    caps->default_dpi.SetSize(devmode->dmPrintQuality, devmode->dmPrintQuality);
    if (devmode->dmFields & DM_YRESOLUTION) {
      caps->default_dpi.set_height(devmode->dmYResolution);
    }
  }
}

}  // namespace

class PrintBackendWin : public PrintBackend {
 public:
  PrintBackendWin() = default;

  // PrintBackend implementation.
  mojom::ResultCode EnumeratePrinters(PrinterList& printer_list) override;
  mojom::ResultCode GetDefaultPrinterName(
      std::string& default_printer) override;
  mojom::ResultCode GetPrinterBasicInfo(
      const std::string& printer_name,
      PrinterBasicInfo* printer_info) override;
  mojom::ResultCode GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) override;
  mojom::ResultCode GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) override;
  std::string GetPrinterDriverInfo(const std::string& printer_name) override;
  bool IsValidPrinter(const std::string& printer_name) override;

 protected:
  ~PrintBackendWin() override = default;
};

mojom::ResultCode PrintBackendWin::EnumeratePrinters(
    PrinterList& printer_list) {
  DCHECK(printer_list.empty());
  DWORD bytes_needed = 0;
  DWORD count_returned = 0;
  constexpr DWORD kFlags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
  const DWORD kLevel = 4;
  EnumPrinters(kFlags, nullptr, kLevel, nullptr, 0, &bytes_needed,
               &count_returned);
  logging::SystemErrorCode code = logging::GetLastSystemErrorCode();
  if (code == ERROR_SUCCESS) {
    // If EnumPrinters() succeeded, that means there are no printer drivers
    // installed because 0 bytes was sufficient.
    DCHECK_EQ(bytes_needed, 0u);
    VLOG(1) << "Found no printers";
    return mojom::ResultCode::kSuccess;
  }

  if (code != ERROR_INSUFFICIENT_BUFFER) {
    LOG(ERROR) << "Error enumerating printers: "
               << logging::SystemErrorCodeToString(code);
    return GetResultCodeFromSystemErrorCode(code);
  }

  auto printer_info_buffer = std::make_unique<BYTE[]>(bytes_needed);
  if (!EnumPrinters(kFlags, nullptr, kLevel, printer_info_buffer.get(),
                    bytes_needed, &bytes_needed, &count_returned)) {
    NOTREACHED();
    return GetResultCodeFromSystemErrorCode(logging::GetLastSystemErrorCode());
  }

  // No need to worry about a query failure for `GetDefaultPrinterName()` here,
  // that would mean we can just treat it as there being no default printer.
  std::string default_printer;
  GetDefaultPrinterName(default_printer);

  PRINTER_INFO_4* printer_info =
      reinterpret_cast<PRINTER_INFO_4*>(printer_info_buffer.get());
  for (DWORD index = 0; index < count_returned; index++) {
    ScopedPrinterHandle printer;
    PrinterBasicInfo info;
    if (printer.OpenPrinterWithName(printer_info[index].pPrinterName) &&
        InitBasicPrinterInfo(printer.Get(), &info)) {
      info.is_default = (info.printer_name == default_printer);
      printer_list.push_back(info);
    }
  }

  VLOG(1) << "Found " << count_returned << " printers";
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendWin::GetDefaultPrinterName(
    std::string& default_printer) {
  DWORD size = MAX_PATH;
  TCHAR default_printer_name[MAX_PATH];
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!::GetDefaultPrinter(default_printer_name, &size)) {
    LOG(ERROR) << "Error getting default printer: "
               << logging::SystemErrorCodeToString(
                      logging::GetLastSystemErrorCode());
    return mojom::ResultCode::kFailed;
  }
  default_printer = base::WideToUTF8(default_printer_name);
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendWin::GetPrinterBasicInfo(
    const std::string& printer_name,
    PrinterBasicInfo* printer_info) {
  ScopedPrinterHandle printer_handle = GetPrinterHandle(printer_name);
  if (!printer_handle.IsValid())
    return GetResultCodeFromSystemErrorCode(logging::GetLastSystemErrorCode());

  if (!InitBasicPrinterInfo(printer_handle.Get(), printer_info)) {
    // InitBasicPrinterInfo() doesn't set a system error code, so just treat as
    // general failure.
    return mojom::ResultCode::kFailed;
  }

  std::string default_printer;
  mojom::ResultCode result = GetDefaultPrinterName(default_printer);
  if (result != mojom::ResultCode::kSuccess) {
    // Query failure means we can treat this printer as not the default.
    printer_info->is_default = false;
  } else {
    printer_info->is_default = (printer_info->printer_name == default_printer);
  }
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendWin::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  ScopedPrinterHandle printer_handle = GetPrinterHandle(printer_name);
  if (!printer_handle.IsValid()) {
    logging::SystemErrorCode err = logging::GetLastSystemErrorCode();
    LOG(WARNING) << "Failed to open printer, error = "
                 << logging::SystemErrorCodeToString(err);
    return GetResultCodeFromSystemErrorCode(err);
  }

  PrinterInfo5 info_5;
  if (!info_5.Init(printer_handle.Get()))
    return GetResultCodeFromSystemErrorCode(logging::GetLastSystemErrorCode());
  const wchar_t* name = info_5.get()->pPrinterName;
  const wchar_t* port = info_5.get()->pPortName;
  DCHECK_EQ(name, base::UTF8ToWide(printer_name));

  PrinterSemanticCapsAndDefaults caps;

  std::unique_ptr<DEVMODE, base::FreeDeleter> user_settings =
      CreateDevMode(printer_handle.Get(), nullptr);
  if (user_settings) {
    caps.color_default = IsDevModeWithColor(user_settings.get());

    if (user_settings->dmFields & DM_DUPLEX) {
      switch (user_settings->dmDuplex) {
        case DMDUP_SIMPLEX:
          caps.duplex_default = mojom::DuplexMode::kSimplex;
          break;
        case DMDUP_VERTICAL:
          caps.duplex_default = mojom::DuplexMode::kLongEdge;
          break;
        case DMDUP_HORIZONTAL:
          caps.duplex_default = mojom::DuplexMode::kShortEdge;
          break;
        default:
          NOTREACHED();
      }
    }

    if (user_settings->dmFields & DM_COLLATE)
      caps.collate_default = (user_settings->dmCollate == DMCOLLATE_TRUE);
  } else {
    LOG(WARNING) << "Fallback to color/simplex mode.";
    caps.color_default = caps.color_changeable;
    caps.duplex_default = mojom::DuplexMode::kSimplex;
  }

  // Get printer capabilities. For more info see here:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd183552(v=vs.85).aspx
  caps.color_changeable =
      (DeviceCapabilities(name, port, DC_COLORDEVICE, nullptr, nullptr) == 1);
  caps.color_model = mojom::ColorModel::kColor;
  caps.bw_model = mojom::ColorModel::kGray;

  caps.duplex_modes.push_back(mojom::DuplexMode::kSimplex);
  if (DeviceCapabilities(name, port, DC_DUPLEX, nullptr, nullptr) == 1) {
    caps.duplex_modes.push_back(mojom::DuplexMode::kLongEdge);
    caps.duplex_modes.push_back(mojom::DuplexMode::kShortEdge);
  }

  caps.collate_capable =
      (DeviceCapabilities(name, port, DC_COLLATE, nullptr, nullptr) == 1);

  caps.copies_max =
      std::max(1, DeviceCapabilities(name, port, DC_COPIES, nullptr, nullptr));

  LoadPaper(name, port, user_settings.get(), &caps);
  LoadDpi(name, port, user_settings.get(), &caps);

  *printer_info = caps;
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendWin::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_info) {
  DCHECK(printer_info);

  ScopedXPSInitializer xps_initializer;
  CHECK(xps_initializer.initialized());

  if (!IsValidPrinter(printer_name))
    return GetResultCodeFromSystemErrorCode(logging::GetLastSystemErrorCode());

  HPTPROVIDER provider = nullptr;
  std::wstring wide_printer_name = base::UTF8ToWide(printer_name);
  HRESULT hr = XPSModule::OpenProvider(wide_printer_name, 1, &provider);
  if (!provider)
    return mojom::ResultCode::kSuccess;

  {
    Microsoft::WRL::ComPtr<IStream> print_capabilities_stream;
    hr = CreateStreamOnHGlobal(nullptr, TRUE, &print_capabilities_stream);
    DCHECK(SUCCEEDED(hr));
    if (print_capabilities_stream.Get()) {
      base::win::ScopedBstr error;
      hr = XPSModule::GetPrintCapabilities(
          provider, nullptr, print_capabilities_stream.Get(), error.Receive());
      DCHECK(SUCCEEDED(hr));
      if (FAILED(hr)) {
        // Failures from getting print capabilities don't give a system error,
        // so just indicate general failure.
        return mojom::ResultCode::kFailed;
      }
      hr = StreamOnHGlobalToString(print_capabilities_stream.Get(),
                                   &printer_info->printer_capabilities);
      DCHECK(SUCCEEDED(hr));
      printer_info->caps_mime_type = "text/xml";
    }
    ScopedPrinterHandle printer_handle;
    if (printer_handle.OpenPrinterWithName(wide_printer_name.c_str())) {
      std::unique_ptr<DEVMODE, base::FreeDeleter> devmode_out(
          CreateDevMode(printer_handle.Get(), nullptr));
      if (!devmode_out) {
        return GetResultCodeFromSystemErrorCode(
            logging::GetLastSystemErrorCode());
      }
      Microsoft::WRL::ComPtr<IStream> printer_defaults_stream;
      hr = CreateStreamOnHGlobal(nullptr, TRUE, &printer_defaults_stream);
      DCHECK(SUCCEEDED(hr));
      if (printer_defaults_stream.Get()) {
        DWORD dm_size = devmode_out->dmSize + devmode_out->dmDriverExtra;
        hr = XPSModule::ConvertDevModeToPrintTicket(
            provider, dm_size, devmode_out.get(), kPTJobScope,
            printer_defaults_stream.Get());
        DCHECK(SUCCEEDED(hr));
        if (SUCCEEDED(hr)) {
          hr = StreamOnHGlobalToString(printer_defaults_stream.Get(),
                                       &printer_info->printer_defaults);
          DCHECK(SUCCEEDED(hr));
          printer_info->defaults_mime_type = "text/xml";
        }
      }
    }
    XPSModule::CloseProvider(provider);
  }
  return mojom::ResultCode::kSuccess;
}

// Gets the information about driver for a specific printer.
std::string PrintBackendWin::GetPrinterDriverInfo(
    const std::string& printer_name) {
  ScopedPrinterHandle printer = GetPrinterHandle(printer_name);
  return printer.IsValid() ? GetDriverInfo(printer.Get()) : std::string();
}

bool PrintBackendWin::IsValidPrinter(const std::string& printer_name) {
  ScopedPrinterHandle printer_handle = GetPrinterHandle(printer_name);
  return printer_handle.IsValid();
}

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstanceImpl(
    const base::Value::Dict* print_backend_settings,
    const std::string& /*locale*/) {
  return base::MakeRefCounted<PrintBackendWin>();
}

base::expected<std::string, mojom::ResultCode>
PrintBackend::GetXmlPrinterCapabilitiesForXpsDriver(
    const std::string& printer_name) {
  ScopedXPSInitializer xps_initializer;
  CHECK(xps_initializer.initialized());

  if (!IsValidPrinter(printer_name)) {
    return base::unexpected(
        GetResultCodeFromSystemErrorCode(logging::GetLastSystemErrorCode()));
  }

  HPTPROVIDER provider = nullptr;
  std::wstring wide_printer_name = base::UTF8ToWide(printer_name);
  HRESULT hr =
      XPSModule::OpenProvider(wide_printer_name, /*version=*/1, &provider);
  ScopedProvider scoped_provider(provider);
  if (FAILED(hr) || !provider) {
    LOG(ERROR) << "Failed to open provider";
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  Microsoft::WRL::ComPtr<IStream> print_capabilities_stream;
  hr = CreateStreamOnHGlobal(/*hGlobal=*/nullptr, /*fDeleteOnRelease=*/TRUE,
                             &print_capabilities_stream);
  if (FAILED(hr) || !print_capabilities_stream.Get()) {
    LOG(ERROR) << "Failed to create stream";
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  base::win::ScopedBstr error;
  hr = XPSModule::GetPrintCapabilities(provider, /*print_ticket=*/nullptr,
                                       print_capabilities_stream.Get(),
                                       error.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get print capabilities";

    // Failures from getting print capabilities don't give a system error,
    // so just indicate general failure.
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  std::string capabilities_xml;
  hr = StreamOnHGlobalToString(print_capabilities_stream.Get(),
                               &capabilities_xml);

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to convert stream to string";
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  DVLOG(2) << "Printer capabilities info: Name = " << printer_name
           << ", capabilities = " << capabilities_xml;
  return capabilities_xml;
}

}  // namespace printing
