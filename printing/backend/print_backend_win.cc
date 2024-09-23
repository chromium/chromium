// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "printing/backend/print_backend_win.h"

#include <objidl.h>
#include <stddef.h>
#include <winspool.h>
#include <wrl/client.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_hglobal.h"
#include "base/win/windows_types.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_info_win.h"
#include "printing/backend/spooler_win.h"
#include "printing/backend/win_helper.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_utils.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

// Wrapper class to close provider automatically.
class ScopedProvider {
 public:
  explicit ScopedProvider(HPTPROVIDER provider) : provider_(provider) {}
  ScopedProvider(const ScopedProvider&) = delete;
  ScopedProvider& operator=(const ScopedProvider&) = delete;

  // Once the object is destroyed, it automatically closes the provider by
  // calling the XPSModule API.
  ~ScopedProvider() {
    if (provider_)
      XPSModule::CloseProvider(provider_);
  }

 private:
  HPTPROVIDER provider_;
};

std::string ErrorMessageCheckSpooler(const std::string& base_message,
                                     logging::SystemErrorCode err) {
  std::string message = base_message;
  if (err != ERROR_SUCCESS) {
    message += logging::SystemErrorCodeToString(err);
  }
  if (internal::IsSpoolerRunning() !=
      internal::SpoolerServiceStatus::kRunning) {
    message += " Windows print spooler is not running";
  } else if (err == ERROR_SUCCESS) {
    message += " unknown internal printing error";
  }
  return message;
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
    out->assign(locked_data.data(), locked_data.size());
  }
  return hr;
}

template <class T>
std::vector<T> GetDeviceCapabilityArray(const wchar_t* printer,
                                        const wchar_t* port,
                                        WORD id) {
  int count = DeviceCapabilities(printer, port, id, nullptr, nullptr);
  if (count <= 0) {
    return {};
  }

  std::vector<T> results;
  results.resize(count * 2);
  count = DeviceCapabilities(printer, port, id,
                             reinterpret_cast<LPTSTR>(results.data()), nullptr);
  if (count <= 0) {
    return {};
  }

  CHECK_LE(static_cast<size_t>(count), results.size());
  results.resize(count);
  return results;
}

gfx::Size GetDefaultDpi(HDC hdc) {
  int dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
  int dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
  return gfx::Size(dpi_x, dpi_y);
}

gfx::Rect LoadPaperPrintableAreaUm(const wchar_t* printer, DEVMODE* devmode) {
  base::win::ScopedCreateDC hdc(
      CreateDC(L"WINSPOOL", printer, nullptr, devmode));

  gfx::Size default_dpi = GetDefaultDpi(hdc.get());

  gfx::Rect printable_area_device_units =
      GetPrintableAreaDeviceUnits(hdc.get());

  // Device units can be non-square, so scale for non-square pixels and convert
  // to microns.
  gfx::Rect printable_area_um =
      gfx::Rect(ConvertUnit(printable_area_device_units.x(),
                            default_dpi.width(), kMicronsPerInch),
                ConvertUnit(printable_area_device_units.y(),
                            default_dpi.height(), kMicronsPerInch),
                ConvertUnit(printable_area_device_units.width(),
                            default_dpi.width(), kMicronsPerInch),
                ConvertUnit(printable_area_device_units.height(),
                            default_dpi.height(), kMicronsPerInch));

  return printable_area_um;
}

// Load the various papers for the printer.  At most the printable area of one
// paper size is loaded.  Returns whether `LoadPaperPrintableAreaUm()` was
// called for the default paper.
bool LoadPaper(const wchar_t* printer,
               const wchar_t* port,
               DEVMODE* devmode,
               PrinterSemanticCapsAndDefaults* caps) {
  static const size_t kToUm = 100;  // Windows uses 0.1mm.
  static const size_t kMaxPaperName = 64;

  struct PaperName {
    wchar_t chars[kMaxPaperName];
  };

  DCHECK_EQ(sizeof(PaperName), sizeof(wchar_t) * kMaxPaperName);

  // Paper
  std::vector<PaperName> names =
      GetDeviceCapabilityArray<PaperName>(printer, port, DC_PAPERNAMES);
  std::vector<POINT> sizes =
      GetDeviceCapabilityArray<POINT>(printer, port, DC_PAPERSIZE);
  std::vector<WORD> ids =
      GetDeviceCapabilityArray<WORD>(printer, port, DC_PAPERS);

  DCHECK_EQ(ids.size(), sizes.size());
  DCHECK_EQ(names.size(), sizes.size());

  if (ids.size() != sizes.size())
    ids.clear();
  if (names.size() != sizes.size())
    names.clear();

  bool loaded_printable_area_from_system = false;
  for (size_t i = 0; i < sizes.size(); ++i) {
    const gfx::Size size_um(sizes[i].x * kToUm, sizes[i].y * kToUm);
    // Skip papers with empty paper sizes.
    if (size_um.IsEmpty()) {
      continue;
    }

    std::string display_name;
    if (!names.empty()) {
      const wchar_t* name_start = names[i].chars;
      std::wstring tmp_name(name_start, kMaxPaperName);
      // Trim trailing zeros.
      tmp_name = tmp_name.c_str();
      display_name = base::WideToUTF8(tmp_name);
    }

    std::string vendor_id;
    gfx::Rect printable_area_um;
    if (!ids.empty()) {
      vendor_id = base::NumberToString(ids[i]);

      // `LoadPaperPrintableAreaUm()` has to create a new device context, which
      // is very expensive for some printer drivers.  Since this is in an
      // inner loop for paper sizes, this can have a significant impact on
      // Print Preview behavior, locking it up with no response for an order
      // of tens of seconds.  This size of impact of this is driver dependent,
      // and in many cases is not of notice for most users.
      //
      // Due to the high cost of some printer drivers, only retrieve the
      // printable area for the default paper size.  Callers can make use of
      // `GetPaperPrintableArea()` to get the printable area for other paper
      // sizes as needed.
      //
      // TODO(crbug.com/40260379):  Remove this limitation compared to other
      // platforms if an alternate way of getting the printable area for all
      // paper sizes can be done without a huge performance penalty.  For
      // now this workaround is only made for in-browser queries.
      if (devmode && (devmode->dmPaperSize == ids[i])) {
        printable_area_um = LoadPaperPrintableAreaUm(printer, devmode);
        loaded_printable_area_from_system = true;
      }
    }

    // Default to the paper size if printable area is missing.
    // We've seen some drivers have a printable area that goes out of bounds of
    // the paper size. In those cases, set the printable area to be the size.
    // (See crbug.com/1412305.)
    const gfx::Rect size_um_rect(size_um);
    if (printable_area_um.IsEmpty() ||
        !size_um_rect.Contains(printable_area_um)) {
      printable_area_um = size_um_rect;
    }

    caps->papers.push_back(PrinterSemanticCapsAndDefaults::Paper(
        display_name, vendor_id, size_um, printable_area_um));
  }

  if (!devmode)
    return loaded_printable_area_from_system;

  // Copy paper with the same ID as default paper.
  if (devmode->dmFields & DM_PAPERSIZE) {
    std::string default_vendor_id = base::NumberToString(devmode->dmPaperSize);
    for (const PrinterSemanticCapsAndDefaults::Paper& paper : caps->papers) {
      if (paper.vendor_id() == default_vendor_id) {
        caps->default_paper = paper;
        break;
      }
    }
  }

  gfx::Size default_size;
  if (devmode->dmFields & DM_PAPERWIDTH)
    default_size.set_width(devmode->dmPaperWidth * kToUm);
  if (devmode->dmFields & DM_PAPERLENGTH)
    default_size.set_height(devmode->dmPaperLength * kToUm);

  // Reset default paper if `dmPaperWidth` or `dmPaperLength` does not match
  // default paper set by `dmPaperSize`.
  if (!default_size.IsEmpty() &&
      default_size != caps->default_paper.size_um()) {
    caps->default_paper = PrinterSemanticCapsAndDefaults::Paper(
        /*display_name=*/"", /*vendor_id=*/"", default_size);
  }

  return loaded_printable_area_from_system;
}

void LoadDpi(const wchar_t* printer,
             const wchar_t* port,
             const DEVMODE* devmode,
             PrinterSemanticCapsAndDefaults* caps) {
  std::vector<POINT> dpis =
      GetDeviceCapabilityArray<POINT>(printer, port, DC_ENUMRESOLUTIONS);
  for (size_t i = 0; i < dpis.size(); ++i)
    caps->dpis.push_back(gfx::Size(dpis[i].x, dpis[i].y));

  if (devmode && (devmode->dmFields & DM_PRINTQUALITY) &&
      devmode->dmPrintQuality > 0) {
    caps->default_dpi.SetSize(devmode->dmPrintQuality, devmode->dmPrintQuality);
    if (devmode->dmFields & DM_YRESOLUTION) {
      caps->default_dpi.set_height(devmode->dmYResolution);
    }
  }

  // If there's no DPI in the list, add the default DPI to the list.
  if (dpis.empty()) {
    if (caps->default_dpi.IsEmpty()) {
      base::win::ScopedCreateDC hdc(
          CreateDC(L"WINSPOOL", printer, nullptr, devmode));
      caps->default_dpi = GetDefaultDpi(hdc.get());
    }
    caps->dpis.push_back(caps->default_dpi);
  }
}

}  // namespace

PrintBackendWin::DriverPaperPrintableArea::DriverPaperPrintableArea(
    const std::string& name,
    const std::vector<std::string>& driver,
    const std::string& id,
    const gfx::Rect& area_um)
    : printer_name(name),
      driver_info(driver),
      vendor_id(id),
      printable_area_um(area_um) {}

PrintBackendWin::DriverPaperPrintableArea::DriverPaperPrintableArea(
    const PrintBackendWin::DriverPaperPrintableArea& other) = default;

PrintBackendWin::DriverPaperPrintableArea::~DriverPaperPrintableArea() =
    default;

PrintBackendWin::PrintBackendWin() = default;

PrintBackendWin::~PrintBackendWin() = default;

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

  auto printer_info_buffer = base::HeapArray<BYTE>::Uninit(bytes_needed);
  if (!EnumPrinters(kFlags, nullptr, kLevel, printer_info_buffer.data(),
                    printer_info_buffer.size(), &bytes_needed,
                    &count_returned)) {
    return GetResultCodeFromSystemErrorCode(logging::GetLastSystemErrorCode());
  }

  // No need to worry about a query failure for `GetDefaultPrinterName()` here,
  // that would mean we can just treat it as there being no default printer.
  std::string default_printer;
  GetDefaultPrinterName(default_printer);

  const auto* printer_info =
      reinterpret_cast<PRINTER_INFO_4*>(printer_info_buffer.data());
  for (DWORD index = 0; index < count_returned; index++) {
    ScopedPrinterHandle printer;
    if (!printer.OpenPrinterWithName(printer_info[index].pPrinterName)) {
      continue;
    }

    std::optional<PrinterBasicInfo> info = GetBasicPrinterInfo(printer.Get());
    if (!info.has_value()) {
      continue;
    }

    info.value().is_default = (info.value().printer_name == default_printer);
    printer_list.push_back(info.value());
  }

  VLOG(1) << "Found " << printer_list.size() << " printers";
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendWin::GetDefaultPrinterName(
    std::string& default_printer) {
  DWORD size = MAX_PATH;
  TCHAR default_printer_name[MAX_PATH];
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!::GetDefaultPrinter(default_printer_name, &size)) {
    logging::SystemErrorCode err = logging::GetLastSystemErrorCode();
    if (err != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << ErrorMessageCheckSpooler("Error getting default printer: ",
                                             err);
      return mojom::ResultCode::kFailed;
    }

    // There is no default printer, which is not treated as a failure.
    default_printer = std::string();
  } else {
    default_printer = base::WideToUTF8(default_printer_name);
  }
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendWin::GetPrinterBasicInfo(
    const std::string& printer_name,
    PrinterBasicInfo* printer_info) {
  ScopedPrinterHandle printer_handle = GetPrinterHandle(printer_name);
  if (!printer_handle.IsValid())
    return GetResultCodeFromSystemErrorCode(logging::GetLastSystemErrorCode());

  std::optional<PrinterBasicInfo> info =
      GetBasicPrinterInfo(printer_handle.Get());
  if (!info.has_value()) {
    // GetBasicPrinterInfo() doesn't set a system error code, so just treat as
    // general failure.
    return mojom::ResultCode::kFailed;
  }

  *printer_info = info.value();
  std::string default_printer;
  mojom::ResultCode result = GetDefaultPrinterName(default_printer);
  if (result == mojom::ResultCode::kSuccess) {
    printer_info->is_default = (printer_info->printer_name == default_printer);
  } else {
    // Query failure means we can treat this printer as not the default.
    printer_info->is_default = false;
  }
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendWin::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  ScopedPrinterHandle printer_handle = GetPrinterHandle(printer_name);
  if (!printer_handle.IsValid()) {
    logging::SystemErrorCode err = logging::GetLastSystemErrorCode();
    LOG(WARNING) << "Failed to open printer `" << printer_name
                 << "`, error = " << logging::SystemErrorCodeToString(err);
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
          // Ignore invalid values to prevent a crash. See crbug.com/359253687
          break;
      }
    }

    if (user_settings->dmFields & DM_COLLATE) {
      caps.collate_default = (user_settings->dmCollate == DMCOLLATE_TRUE);
    }
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

  bool loaded_printable_area =
      LoadPaper(name, port, user_settings.get(), &caps);
  LoadDpi(name, port, user_settings.get(), &caps);

  if (loaded_printable_area) {
    // Save the printable area of the default paper that was loaded.  Any
    // subsequent call to `GetPaperPrintableArea()` can reuse this value so
    // long as the printer/paper hasn't changed.
    last_default_paper_printable_area_ = DriverPaperPrintableArea(
        printer_name, GetDriverInfo(printer_handle.Get()),
        caps.default_paper.vendor_id(), caps.default_paper.printable_area_um());

    if (printable_area_loaded_callback_for_test_) {
      printable_area_loaded_callback_for_test_.Run();
    }
  }

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

std::optional<gfx::Rect> PrintBackendWin::GetPaperPrintableArea(
    const std::string& printer_name,
    const std::string& paper_vendor_id,
    const gfx::Size& paper_size_um) {
  ScopedPrinterHandle printer_handle = GetPrinterHandle(printer_name);
  if (!printer_handle.IsValid()) {
    return std::nullopt;
  }

  // Reuse the paper printable area loaded with rest of printer capabilities
  // if possible.
  if (last_default_paper_printable_area_.has_value() &&
      last_default_paper_printable_area_.value().printer_name == printer_name &&
      last_default_paper_printable_area_.value().driver_info ==
          GetDriverInfo(printer_handle.Get()) &&
      last_default_paper_printable_area_.value().vendor_id == paper_vendor_id) {
    return last_default_paper_printable_area_.value().printable_area_um;
  }

  std::unique_ptr<DEVMODE, base::FreeDeleter> devmode =
      CreateDevMode(printer_handle.Get(), nullptr);
  if (!devmode) {
    return std::nullopt;
  }

  unsigned id = 0;
  // If the paper size is a custom user size, setting by ID may not work.
  if (base::StringToUint(paper_vendor_id, &id) && id && id < DMPAPER_USER) {
    devmode->dmFields |= DM_PAPERSIZE;
    devmode->dmPaperSize = static_cast<short>(id);
  } else if (!paper_size_um.IsEmpty()) {
    static constexpr int kTenthsOfMillimetersPerInch = 254;
    devmode->dmFields |= DM_PAPERWIDTH | DM_PAPERLENGTH;
    devmode->dmPaperWidth = ConvertUnit(paper_size_um.width(), kMicronsPerInch,
                                        kTenthsOfMillimetersPerInch);
    devmode->dmPaperLength = ConvertUnit(
        paper_size_um.height(), kMicronsPerInch, kTenthsOfMillimetersPerInch);
  }

  if (printable_area_loaded_callback_for_test_) {
    printable_area_loaded_callback_for_test_.Run();
  }

  return LoadPaperPrintableAreaUm(base::UTF8ToWide(printer_name).c_str(),
                                  devmode.get());
}

// Gets the information about driver for a specific printer.
std::vector<std::string> PrintBackendWin::GetPrinterDriverInfo(
    const std::string& printer_name) {
  ScopedPrinterHandle printer = GetPrinterHandle(printer_name);
  return printer.IsValid() ? GetDriverInfo(printer.Get())
                           : std::vector<std::string>();
}

bool PrintBackendWin::IsValidPrinter(const std::string& printer_name) {
  ScopedPrinterHandle printer_handle = GetPrinterHandle(printer_name);
  return printer_handle.IsValid();
}

void PrintBackendWin::SetPrintableAreaLoadedCallbackForTesting(
    base::RepeatingClosure callback) {
  printable_area_loaded_callback_for_test_ = std::move(callback);
}

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstanceImpl(
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
