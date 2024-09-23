// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_setup.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_conversion.h"
#include "printing/printing_context_factory_for_test.h"
#include "printing/units.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "printing/printing_features.h"
#endif

namespace printing {

namespace {

PrintingContextFactoryForTest* g_printing_context_factory_for_test = nullptr;

}  // namespace

PrintingContext::PrintingContext(Delegate* delegate,
                                 ProcessBehavior process_behavior)
    : settings_(std::make_unique<PrintSettings>()),
      delegate_(delegate),
      in_print_job_(false),
      abort_printing_(false),
      process_behavior_(process_behavior) {
  DCHECK(delegate_);
}

PrintingContext::~PrintingContext() = default;

// static
std::unique_ptr<PrintingContext> PrintingContext::Create(
    Delegate* delegate,
    ProcessBehavior process_behavior) {
  return g_printing_context_factory_for_test
             ? g_printing_context_factory_for_test->CreatePrintingContext(
                   delegate, process_behavior)
             : PrintingContext::CreateImpl(delegate, process_behavior);
}

// static
void PrintingContext::SetPrintingContextFactoryForTest(
    PrintingContextFactoryForTest* factory) {
  g_printing_context_factory_for_test = factory;
}

void PrintingContext::set_margin_type(mojom::MarginType type) {
  DCHECK(type != mojom::MarginType::kCustomMargins);
  settings_->set_margin_type(type);
}

void PrintingContext::set_is_modifiable(bool is_modifiable) {
  settings_->set_is_modifiable(is_modifiable);
}

const PrintSettings& PrintingContext::settings() const {
  DCHECK(!in_print_job_);
  return *settings_;
}

void PrintingContext::ResetSettings() {
  ReleaseContext();

  settings_->Clear();

  in_print_job_ = false;
  abort_printing_ = false;
}

std::unique_ptr<PrintSettings> PrintingContext::TakeAndResetSettings() {
  std::unique_ptr<PrintSettings> result = std::move(settings_);
  settings_ = std::make_unique<PrintSettings>();
  return result;
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)
void PrintingContext::SetJobId(int job_id) {
  // Should only use this method to update the browser `PrintingContext` with
  // the value provided by the PrintBackend service.
  CHECK_EQ(process_behavior_, ProcessBehavior::kOopEnabledSkipSystemCalls);
  job_id_ = job_id;
}
#endif

mojom::ResultCode PrintingContext::OnError() {
  mojom::ResultCode result = abort_printing_ ? mojom::ResultCode::kCanceled
                                             : mojom::ResultCode::kFailed;
  ResetSettings();
  return result;
}

void PrintingContext::SetDefaultPrintableAreaForVirtualPrinters() {
  gfx::Size paper_size(GetPdfPaperSizeDeviceUnits());
  if (!settings_->requested_media().size_microns.IsEmpty()) {
    float device_microns_per_device_unit = static_cast<float>(kMicronsPerInch) /
                                           settings_->device_units_per_inch();
    paper_size = gfx::Size(settings_->requested_media().size_microns.width() /
                               device_microns_per_device_unit,
                           settings_->requested_media().size_microns.height() /
                               device_microns_per_device_unit);
  }
  gfx::Rect paper_rect(0, 0, paper_size.width(), paper_size.height());
  settings_->SetPrinterPrintableArea(paper_size, paper_rect,
                                     /*landscape_needs_flip=*/true);
}

void PrintingContext::UsePdfSettings() {
  base::Value::Dict pdf_settings;
  pdf_settings.Set(kSettingHeaderFooterEnabled, false);
  pdf_settings.Set(kSettingShouldPrintBackgrounds, false);
  pdf_settings.Set(kSettingShouldPrintSelectionOnly, false);
  pdf_settings.Set(kSettingMarginsType,
                   static_cast<int>(mojom::MarginType::kNoMargins));
  pdf_settings.Set(kSettingCollate, true);
  pdf_settings.Set(kSettingCopies, 1);
  pdf_settings.Set(kSettingColor, static_cast<int>(mojom::ColorModel::kColor));
  // DPI value should match GetPdfCapabilities().
  pdf_settings.Set(kSettingDpiHorizontal, kDefaultPdfDpi);
  pdf_settings.Set(kSettingDpiVertical, kDefaultPdfDpi);
  pdf_settings.Set(kSettingDuplexMode,
                   static_cast<int>(printing::mojom::DuplexMode::kSimplex));
  pdf_settings.Set(kSettingLandscape, false);
  pdf_settings.Set(kSettingDeviceName, "");
  pdf_settings.Set(kSettingPrinterType,
                   static_cast<int>(mojom::PrinterType::kPdf));
  pdf_settings.Set(kSettingScaleFactor, 100);
  pdf_settings.Set(kSettingRasterizePdf, false);
  pdf_settings.Set(kSettingPagesPerSheet, 1);
  mojom::ResultCode result = UpdatePrintSettings(std::move(pdf_settings));
  // TODO(thestig): Downgrade these to DCHECKs after shipping these CHECKs to
  // production without any failures.
  CHECK_EQ(result, mojom::ResultCode::kSuccess);
  // UsePdfSettings() should never fail and the returned DPI should always be a
  // well-known value that is safe to use as a divisor.
#if BUILDFLAG(IS_MAC)
  CHECK_EQ(settings_->device_units_per_inch(), kPointsPerInch);
#else
  CHECK_EQ(settings_->device_units_per_inch(), kDefaultPdfDpi);
#endif
}

mojom::ResultCode PrintingContext::UpdatePrintSettings(
    base::Value::Dict job_settings) {
  ResetSettings();
  {
    std::unique_ptr<PrintSettings> settings =
        PrintSettingsFromJobSettings(job_settings);
    if (!settings) {
      DUMP_WILL_BE_NOTREACHED();
      return OnError();
    }
    settings_ = std::move(settings);
  }

  mojom::PrinterType printer_type = static_cast<mojom::PrinterType>(
      job_settings.FindInt(kSettingPrinterType).value());
  if (printer_type == mojom::PrinterType::kPrivetDeprecated ||
      printer_type == mojom::PrinterType::kCloudDeprecated) {
    NOTREACHED();
  }

  bool open_in_external_preview =
      job_settings.contains(kSettingOpenPDFInPreview);

  if (!open_in_external_preview &&
      (printer_type == mojom::PrinterType::kPdf ||
       printer_type == mojom::PrinterType::kExtension)) {
    if (settings_->page_setup_device_units().printable_area().IsEmpty())
      SetDefaultPrintableAreaForVirtualPrinters();
    return mojom::ResultCode::kSuccess;
  }

  // The `open_in_external_preview` case does not care about the printable area.
  // Local printers set their printable area within UpdatePrinterSettings().
  DCHECK(open_in_external_preview ||
         printer_type == mojom::PrinterType::kLocal);

  PrinterSettings printer_settings {
#if BUILDFLAG(IS_MAC)
    .external_preview = open_in_external_preview,
#endif
    .show_system_dialog =
        job_settings.FindBool(kSettingShowSystemDialog).value_or(false),
#if BUILDFLAG(IS_WIN)
    .page_count = job_settings.FindInt(kSettingPreviewPageCount).value_or(0)
#endif
  };
  return UpdatePrinterSettings(printer_settings);
}

#if BUILDFLAG(IS_CHROMEOS)
mojom::ResultCode PrintingContext::UpdatePrintSettingsFromPOD(
    std::unique_ptr<PrintSettings> job_settings) {
  ResetSettings();
  settings_ = std::move(job_settings);

  return UpdatePrinterSettings({.show_system_dialog = false});
}
#endif

void PrintingContext::SetPrintSettings(const PrintSettings& settings) {
  *settings_ = settings;
}

}  // namespace printing
