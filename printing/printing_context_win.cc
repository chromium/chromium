// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "printing/printing_context_win.h"

#include <winspool.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/free_deleter.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/win_helper.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_setup.h"
#include "printing/print_settings_initializer_win.h"
#include "printing/printed_document.h"
#include "printing/printed_page_win.h"
#include "printing/printing_context_system_dialog_win.h"
#include "printing/printing_features.h"
#include "printing/printing_utils.h"
#include "printing/units.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace printing {

namespace {

// Helper class to ensure that a saved device context state gets restored at end
// of scope.
class ScopedSavedState {
 public:
  ScopedSavedState(HDC context)
      : context_(context), saved_state_(SaveDC(context)) {
    DCHECK_NE(saved_state_, 0);
  }
  ~ScopedSavedState() {
    BOOL res = RestoreDC(context_, saved_state_);
    DCHECK_NE(res, 0);
  }

 private:
  HDC context_;
  int saved_state_;
};

void AssignResult(mojom::ResultCode* out, mojom::ResultCode in) {
  *out = in;
}

void SimpleModifyWorldTransform(HDC context,
                                int offset_x,
                                int offset_y,
                                float shrink_factor) {
  XFORM xform = {0};
  xform.eDx = static_cast<float>(offset_x);
  xform.eDy = static_cast<float>(offset_y);
  xform.eM11 = xform.eM22 = 1.f / shrink_factor;
  BOOL res = ModifyWorldTransform(context, &xform, MWT_LEFTMULTIPLY);
  DCHECK_NE(res, 0);
}

}  // namespace

// static
std::unique_ptr<PrintingContext> PrintingContext::CreateImpl(
    Delegate* delegate,
    ProcessBehavior process_behavior) {
  return std::make_unique<PrintingContextSystemDialogWin>(delegate,
                                                          process_behavior);
}

PrintingContextWin::PrintingContextWin(Delegate* delegate,
                                       ProcessBehavior process_behavior)
    : PrintingContext(delegate, process_behavior), context_(nullptr) {}

PrintingContextWin::~PrintingContextWin() {
  ReleaseContext();
}

void PrintingContextWin::AskUserForSettings(int max_pages,
                                            bool has_selection,
                                            bool is_scripted,
                                            PrintSettingsCallback callback) {
  NOTIMPLEMENTED();
}

mojom::ResultCode PrintingContextWin::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  scoped_refptr<PrintBackend> backend =
      PrintBackend::CreateInstance(delegate_->GetAppLocale());
  std::string default_printer_name;
  mojom::ResultCode result =
      backend->GetDefaultPrinterName(default_printer_name);
  if (result != mojom::ResultCode::kSuccess)
    return result;

  std::wstring default_printer = base::UTF8ToWide(default_printer_name);
  if (!default_printer.empty()) {
    ScopedPrinterHandle printer;
    if (printer.OpenPrinterWithName(default_printer.c_str())) {
      std::unique_ptr<DEVMODE, base::FreeDeleter> dev_mode =
          CreateDevMode(printer.Get(), nullptr);
      if (InitializeSettings(default_printer, dev_mode.get()) ==
          mojom::ResultCode::kSuccess) {
        return mojom::ResultCode::kSuccess;
      }
    }
  }

  ReleaseContext();

  // No default printer configured, do we have any printers at all?
  DWORD bytes_needed = 0;
  DWORD count_returned = 0;
  (void)::EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr,
                       2, nullptr, 0, &bytes_needed, &count_returned);
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

  DCHECK_GE(bytes_needed, count_returned * sizeof(PRINTER_INFO_2));
  std::vector<BYTE> printer_info_buffer(bytes_needed);
  BOOL ret = ::EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
                            nullptr, 2, printer_info_buffer.data(),
                            bytes_needed, &bytes_needed, &count_returned);
  if (ret && count_returned) {  // have printers
    // Open the first successfully found printer.
    const PRINTER_INFO_2* info_2 =
        reinterpret_cast<PRINTER_INFO_2*>(printer_info_buffer.data());
    const PRINTER_INFO_2* info_2_end = info_2 + count_returned;
    for (; info_2 < info_2_end; ++info_2) {
      ScopedPrinterHandle printer;
      if (!printer.OpenPrinterWithName(info_2->pPrinterName)) {
        continue;
      }
      std::unique_ptr<DEVMODE, base::FreeDeleter> dev_mode =
          CreateDevMode(printer.Get(), nullptr);
      if (InitializeSettings(info_2->pPrinterName, dev_mode.get()) ==
          mojom::ResultCode::kSuccess) {
        return mojom::ResultCode::kSuccess;
      }
    }
    if (context_) {
      return mojom::ResultCode::kSuccess;
    }
  }

  return OnError();
}

gfx::Size PrintingContextWin::GetPdfPaperSizeDeviceUnits() {
  // Default fallback to Letter size.
  gfx::SizeF paper_size(kLetterWidthInch, kLetterHeightInch);

  // Get settings from locale. Paper type buffer length is at most 4.
  const int paper_type_buffer_len = 4;
  wchar_t paper_type_buffer[paper_type_buffer_len] = {0};
  GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IPAPERSIZE, paper_type_buffer,
                paper_type_buffer_len);
  if (wcslen(paper_type_buffer)) {  // The call succeeded.
    int paper_code = _wtoi(paper_type_buffer);
    switch (paper_code) {
      case DMPAPER_LEGAL:
        paper_size.SetSize(kLegalWidthInch, kLegalHeightInch);
        break;
      case DMPAPER_A4:
        paper_size.SetSize(kA4WidthInch, kA4HeightInch);
        break;
      case DMPAPER_A3:
        paper_size.SetSize(kA3WidthInch, kA3HeightInch);
        break;
      default:  // DMPAPER_LETTER is used for default fallback.
        break;
    }
  }
  return gfx::Size(paper_size.width() * settings_->device_units_per_inch(),
                   paper_size.height() * settings_->device_units_per_inch());
}

mojom::ResultCode PrintingContextWin::UpdatePrinterSettings(
    const PrinterSettings& printer_settings) {
  DCHECK(!in_print_job_);

  ScopedPrinterHandle printer;
  if (!printer.OpenPrinterWithName(base::as_wcstr(settings_->device_name())))
    return OnError();

  // Make printer changes local to Chrome.
  // See MSDN documentation regarding DocumentProperties.
  std::unique_ptr<DEVMODE, base::FreeDeleter> scoped_dev_mode =
      CreateDevModeWithColor(printer.Get(),
                             base::UTF16ToWide(settings_->device_name()),
                             settings_->color() != mojom::ColorModel::kGray);
  if (!scoped_dev_mode)
    return OnError();

  {
    DEVMODE* dev_mode = scoped_dev_mode.get();
    dev_mode->dmCopies = std::max(settings_->copies(), 1);
    if (dev_mode->dmCopies > 1) {  // do not change unless multiple copies
      dev_mode->dmFields |= DM_COPIES;
      dev_mode->dmCollate =
          settings_->collate() ? DMCOLLATE_TRUE : DMCOLLATE_FALSE;
    }

    switch (settings_->duplex_mode()) {
      case mojom::DuplexMode::kLongEdge:
        dev_mode->dmFields |= DM_DUPLEX;
        dev_mode->dmDuplex = DMDUP_VERTICAL;
        break;
      case mojom::DuplexMode::kShortEdge:
        dev_mode->dmFields |= DM_DUPLEX;
        dev_mode->dmDuplex = DMDUP_HORIZONTAL;
        break;
      case mojom::DuplexMode::kSimplex:
        dev_mode->dmFields |= DM_DUPLEX;
        dev_mode->dmDuplex = DMDUP_SIMPLEX;
        break;
      default:  // kUnknownDuplexMode
        break;
    }

    dev_mode->dmFields |= DM_ORIENTATION;
    dev_mode->dmOrientation =
        settings_->landscape() ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;

    if (settings_->dpi_horizontal() > 0) {
      dev_mode->dmPrintQuality = settings_->dpi_horizontal();
      dev_mode->dmFields |= DM_PRINTQUALITY;
    }
    if (settings_->dpi_vertical() > 0) {
      dev_mode->dmYResolution = settings_->dpi_vertical();
      dev_mode->dmFields |= DM_YRESOLUTION;
    }

    const PrintSettings::RequestedMedia& requested_media =
        settings_->requested_media();
    unsigned id = 0;
    // If the paper size is a custom user size, setting by ID may not work.
    if (base::StringToUint(requested_media.vendor_id, &id) && id &&
        id < DMPAPER_USER) {
      dev_mode->dmFields |= DM_PAPERSIZE;
      dev_mode->dmPaperSize = static_cast<short>(id);
    } else if (!requested_media.size_microns.IsEmpty()) {
      static constexpr int kFromUm = 100;  // Windows uses 0.1mm.
      dev_mode->dmFields |= DM_PAPERWIDTH | DM_PAPERLENGTH;
      dev_mode->dmPaperWidth = requested_media.size_microns.width() / kFromUm;
      dev_mode->dmPaperLength = requested_media.size_microns.height() / kFromUm;
    }
  }

  // Update data using DocumentProperties.
  if (printer_settings.show_system_dialog) {
    mojom::ResultCode result = mojom::ResultCode::kFailed;
    AskUserForSettings(printer_settings.page_count, /*has_selection=*/false,
                       /*is_scripted=*/false,
                       base::BindOnce(&AssignResult, &result));
    return result;
  }
  // Set printer then refresh printer settings.
  scoped_dev_mode = CreateDevMode(printer.Get(), scoped_dev_mode.get());
  if (!scoped_dev_mode)
    return OnError();

  // Since CreateDevMode() doesn't honor color settings through the GDI call
  // to DocumentProperties(), ensure the requested values persist here.
  scoped_dev_mode->dmFields |= DM_COLOR;
  scoped_dev_mode->dmColor = settings_->color() != mojom::ColorModel::kGray
                                 ? DMCOLOR_COLOR
                                 : DMCOLOR_MONOCHROME;

  return InitializeSettings(base::UTF16ToWide(settings_->device_name()),
                            scoped_dev_mode.get());
}

mojom::ResultCode PrintingContextWin::InitWithSettingsForTest(
    std::unique_ptr<PrintSettings> settings) {
  DCHECK(!in_print_job_);

  settings_ = std::move(settings);

  // TODO(maruel): settings_.ToDEVMODE()
  ScopedPrinterHandle printer;
  if (!printer.OpenPrinterWithName(base::as_wcstr(settings_->device_name()))) {
    return logging::GetLastSystemErrorCode() == ERROR_ACCESS_DENIED
               ? mojom::ResultCode::kAccessDenied
               : mojom::ResultCode::kFailed;
  }

  std::unique_ptr<DEVMODE, base::FreeDeleter> dev_mode =
      CreateDevMode(printer.Get(), nullptr);

  return InitializeSettings(base::UTF16ToWide(settings_->device_name()),
                            dev_mode.get());
}

mojom::ResultCode PrintingContextWin::NewDocument(
    const std::u16string& document_name) {
  DCHECK(!in_print_job_);
  if (!context_
#if BUILDFLAG(ENABLE_OOP_PRINTING)
      && process_behavior() != ProcessBehavior::kOopEnabledSkipSystemCalls
#endif
  ) {
    return OnError();
  }

  // Set the flag used by the AbortPrintJob dialog procedure.
  abort_printing_ = false;

  in_print_job_ = true;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (process_behavior() == ProcessBehavior::kOopEnabledSkipSystemCalls) {
    return mojom::ResultCode::kSuccess;
  }
#endif

  if (base::FeatureList::IsEnabled(printing::features::kUseXpsForPrinting)) {
    // This is all the new document context needed when using XPS.
    return mojom::ResultCode::kSuccess;
  }

  // Need more context setup when using GDI.

  // Register the application's AbortProc function with GDI.
  if (SP_ERROR == SetAbortProc(context_, &AbortProc))
    return OnError();

  DCHECK(SimplifyDocumentTitle(document_name) == document_name);
  DOCINFO di = {sizeof(DOCINFO)};
  di.lpszDocName = base::as_wcstr(document_name);

  // Is there a debug dump directory specified? If so, force to print to a file.
  if (PrintedDocument::HasDebugDumpPath()) {
    base::FilePath debug_dump_path = PrintedDocument::CreateDebugDumpPath(
        document_name, FILE_PATH_LITERAL(".prn"));
    if (!debug_dump_path.empty())
      di.lpszOutput = debug_dump_path.value().c_str();
  }

  // No message loop running in unit tests.
  DCHECK(
      !base::CurrentThread::Get() ||
      !base::CurrentThread::Get()->ApplicationTasksAllowedInNativeNestedLoop());

  // Begin a print job by calling the StartDoc function.
  // NOTE: StartDoc() starts a message loop. That causes a lot of problems with
  // IPC. Make sure recursive task processing is disabled.
  if (StartDoc(context_, &di) <= 0)
    return OnError();

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextWin::RenderPage(const PrintedPage& page,
                                                 const PageSetup& page_setup) {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(context_);
  DCHECK(in_print_job_);

  gfx::Rect content_area = GetCenteredPageContentRect(
      page_setup.physical_size(), page.page_size(), page.page_content_rect());

  // Save the state to make sure the context this function call does not modify
  // the device context.
  ScopedSavedState saved_state(context_);
  skia::InitializeDC(context_);
  {
    // Save the state (again) to apply the necessary world transformation.
    ScopedSavedState saved_state_inner(context_);

    // Setup the matrix to translate and scale to the right place. Take in
    // account the actual shrinking factor.
    // Note that the printing output is relative to printable area of the page.
    // That is 0,0 is offset by PHYSICALOFFSETX/Y from the page.
    SimpleModifyWorldTransform(
        context_, content_area.x() - page_setup.printable_area().x(),
        content_area.y() - page_setup.printable_area().y(),
        page.shrink_factor());

    if (::StartPage(context_) <= 0)
      return mojom::ResultCode::kFailed;
    bool played_back = page.metafile()->SafePlayback(context_);
    DCHECK(played_back);
    if (::EndPage(context_) <= 0)
      return mojom::ResultCode::kFailed;
  }

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextWin::PrintDocument(
    const MetafilePlayer& metafile,
    const PrintSettings& settings,
    uint32_t num_pages) {
  // TODO(crbug.com/40100562)
  NOTIMPLEMENTED();
  return mojom::ResultCode::kFailed;
}

mojom::ResultCode PrintingContextWin::DocumentDone() {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);
  DCHECK(context_);

  // Inform the driver that document has ended.
  if (EndDoc(context_) <= 0)
    return OnError();

  ResetSettings();
  return mojom::ResultCode::kSuccess;
}

void PrintingContextWin::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  if (context_)
    CancelDC(context_);
}

void PrintingContextWin::ReleaseContext() {
  if (context_) {
    DeleteDC(context_);
    context_ = nullptr;
  }
}

printing::NativeDrawingContext PrintingContextWin::context() const {
  return context_;
}

// static
BOOL PrintingContextWin::AbortProc(HDC hdc, int nCode) {
  if (nCode) {
    // TODO(maruel):  Need a way to find the right instance to set. Should
    // leverage PrintJobManager here?
    // abort_printing_ = true;
  }
  return true;
}

mojom::ResultCode PrintingContextWin::InitializeSettings(
    const std::wstring& device_name,
    DEVMODE* dev_mode) {
  if (!dev_mode)
    return OnError();

  ReleaseContext();
  context_ = CreateDC(L"WINSPOOL", device_name.c_str(), nullptr, dev_mode);
  if (!context_)
    return OnError();

  skia::InitializeDC(context_);

  DCHECK(!in_print_job_);
  settings_->set_device_name(base::WideToUTF16(device_name));
  PrintSettingsInitializerWin::InitPrintSettings(context_, *dev_mode,
                                                 settings_.get());

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextWin::OnError() {
  mojom::ResultCode result;
  if (abort_printing_) {
    result = mojom::ResultCode::kCanceled;
  } else {
    switch (logging::GetLastSystemErrorCode()) {
      case ERROR_ACCESS_DENIED:
        result = mojom::ResultCode::kAccessDenied;
        break;
      case ERROR_CANCELLED:
        result = mojom::ResultCode::kCanceled;
        break;
      default:
        result = mojom::ResultCode::kFailed;
        break;
    }
  }
  ResetSettings();
  return result;
}

HWND PrintingContextWin::GetRootWindow(gfx::NativeView view) {
  HWND window = nullptr;
  if (view && view->GetHost())
    window = view->GetHost()->GetAcceleratedWidget();
  if (!window) {
    // TODO(maruel):  b/1214347 Get the right browser window instead.
    return GetDesktopWindow();
  }
  return window;
}

}  // namespace printing
