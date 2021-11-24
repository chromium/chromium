// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
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

const float kCloudPrintMarginInch = 0.25;
}

PrintingContext::PrintingContext(Delegate* delegate)
    : settings_(std::make_unique<PrintSettings>()),
      delegate_(delegate),
      in_print_job_(false),
      abort_printing_(false) {
  DCHECK(delegate_);
}

PrintingContext::~PrintingContext() = default;

// static
std::unique_ptr<PrintingContext> PrintingContext::Create(
    Delegate* delegate,
    bool skip_system_calls) {
  return g_printing_context_factory_for_test
             ? g_printing_context_factory_for_test->CreatePrintingContext(
                   delegate, skip_system_calls)
             : PrintingContext::CreateImpl(delegate, skip_system_calls);
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

mojom::ResultCode PrintingContext::OnError() {
  mojom::ResultCode result = abort_printing_ ? mojom::ResultCode::kCanceled
                                             : mojom::ResultCode::kFailed;
  ResetSettings();
  return result;
}

mojom::ResultCode PrintingContext::UsePdfSettings() {
  base::Value pdf_settings(base::Value::Type::DICTIONARY);
  pdf_settings.SetBoolKey(kSettingHeaderFooterEnabled, false);
  pdf_settings.SetBoolKey(kSettingShouldPrintBackgrounds, false);
  pdf_settings.SetBoolKey(kSettingShouldPrintSelectionOnly, false);
  pdf_settings.SetIntKey(kSettingMarginsType,
                         static_cast<int>(mojom::MarginType::kNoMargins));
  pdf_settings.SetBoolKey(kSettingCollate, true);
  pdf_settings.SetIntKey(kSettingCopies, 1);
  pdf_settings.SetIntKey(kSettingColor,
                         static_cast<int>(mojom::ColorModel::kColor));
  pdf_settings.SetIntKey(kSettingDpiHorizontal, kPointsPerInch);
  pdf_settings.SetIntKey(kSettingDpiVertical, kPointsPerInch);
  pdf_settings.SetIntKey(
      kSettingDuplexMode,
      static_cast<int>(printing::mojom::DuplexMode::kSimplex));
  pdf_settings.SetBoolKey(kSettingLandscape, false);
  pdf_settings.SetStringKey(kSettingDeviceName, "");
  pdf_settings.SetIntKey(kSettingPrinterType,
                         static_cast<int>(mojom::PrinterType::kPdf));
  pdf_settings.SetIntKey(kSettingScaleFactor, 100);
  pdf_settings.SetBoolKey(kSettingRasterizePdf, false);
  pdf_settings.SetIntKey(kSettingPagesPerSheet, 1);
  return UpdatePrintSettings(std::move(pdf_settings));
}

mojom::ResultCode PrintingContext::UpdatePrintSettings(
    base::Value job_settings) {
  ResetSettings();
  {
    std::unique_ptr<PrintSettings> settings =
        PrintSettingsFromJobSettings(job_settings);
    if (!settings) {
      NOTREACHED();
      return OnError();
    }
    settings_ = std::move(settings);
  }

  mojom::PrinterType printer_type = static_cast<mojom::PrinterType>(
      job_settings.FindIntKey(kSettingPrinterType).value());
  bool print_with_privet = printer_type == mojom::PrinterType::kPrivet;
  bool print_to_cloud = !!job_settings.FindKey(kSettingCloudPrintId);
  bool open_in_external_preview =
      !!job_settings.FindKey(kSettingOpenPDFInPreview);

  if (!open_in_external_preview &&
      (print_to_cloud || print_with_privet ||
       printer_type == mojom::PrinterType::kPdf ||
       printer_type == mojom::PrinterType::kCloud ||
       printer_type == mojom::PrinterType::kExtension)) {
    settings_->set_dpi(kDefaultPdfDpi);
    gfx::Size paper_size(GetPdfPaperSizeDeviceUnits());
    if (!settings_->requested_media().size_microns.IsEmpty()) {
      float device_microns_per_device_unit =
          static_cast<float>(kMicronsPerInch) /
          settings_->device_units_per_inch();
      paper_size =
          gfx::Size(settings_->requested_media().size_microns.width() /
                        device_microns_per_device_unit,
                    settings_->requested_media().size_microns.height() /
                        device_microns_per_device_unit);
    }
    gfx::Rect paper_rect(0, 0, paper_size.width(), paper_size.height());
    if (print_to_cloud || print_with_privet) {
      paper_rect.Inset(
          kCloudPrintMarginInch * settings_->device_units_per_inch(),
          kCloudPrintMarginInch * settings_->device_units_per_inch());
    }
    settings_->SetPrinterPrintableArea(paper_size, paper_rect, true);
    return mojom::ResultCode::kSuccess;
  }

  PrinterSettings printer_settings {
#if defined(OS_MAC)
    .external_preview = open_in_external_preview,
#endif
    .show_system_dialog =
        job_settings.FindBoolKey(kSettingShowSystemDialog).value_or(false),
#if defined(OS_WIN)
    .page_count = job_settings.FindIntKey(kSettingPreviewPageCount).value_or(0)
#endif
  };
  return UpdatePrinterSettings(printer_settings);
}

#if defined(OS_CHROMEOS)
mojom::ResultCode PrintingContext::UpdatePrintSettingsFromPOD(
    std::unique_ptr<PrintSettings> job_settings) {
  ResetSettings();
  settings_ = std::move(job_settings);

  return UpdatePrinterSettings({.show_system_dialog = false});
}
#endif

void PrintingContext::ApplyPrintSettings(const PrintSettings& settings) {
  *settings_ = settings;
}

}  // namespace printing
