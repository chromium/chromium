// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include <utility>

#include "base/logging.h"
#include "printing/page_setup.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_conversion.h"
#include "printing/units.h"

namespace printing {

namespace {
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

void PrintingContext::set_margin_type(MarginType type) {
  DCHECK(type != CUSTOM_MARGINS);
  settings_->set_margin_type(type);
}

void PrintingContext::set_is_modifiable(bool is_modifiable) {
  settings_->set_is_modifiable(is_modifiable);
#if defined(OS_WIN)
  settings_->set_print_text_with_gdi(is_modifiable);
#endif
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

PrintingContext::Result PrintingContext::OnError() {
  Result result = abort_printing_ ? CANCEL : FAILED;
  ResetSettings();
  return result;
}

PrintingContext::Result PrintingContext::UsePdfSettings() {
  base::Value pdf_settings(base::Value::Type::DICTIONARY);
  pdf_settings.SetBoolKey(kSettingHeaderFooterEnabled, false);
  pdf_settings.SetBoolKey(kSettingShouldPrintBackgrounds, false);
  pdf_settings.SetBoolKey(kSettingShouldPrintSelectionOnly, false);
  pdf_settings.SetIntKey(kSettingMarginsType, printing::NO_MARGINS);
  pdf_settings.SetBoolKey(kSettingCollate, true);
  pdf_settings.SetIntKey(kSettingCopies, 1);
  pdf_settings.SetIntKey(kSettingColor, printing::COLOR);
  pdf_settings.SetIntKey(kSettingDpiHorizontal, kPointsPerInch);
  pdf_settings.SetIntKey(kSettingDpiVertical, kPointsPerInch);
  pdf_settings.SetIntKey(kSettingDuplexMode, printing::SIMPLEX);
  pdf_settings.SetBoolKey(kSettingLandscape, false);
  pdf_settings.SetStringKey(kSettingDeviceName, "");
  pdf_settings.SetIntKey(kSettingPrinterType, kPdfPrinter);
  pdf_settings.SetIntKey(kSettingScaleFactor, 100);
  pdf_settings.SetBoolKey(kSettingRasterizePdf, false);
  pdf_settings.SetIntKey(kSettingPagesPerSheet, 1);
  return UpdatePrintSettings(std::move(pdf_settings));
}

PrintingContext::Result PrintingContext::UpdatePrintSettings(
    base::Value job_settings) {
  ResetSettings();

  if (!PrintSettingsFromJobSettings(job_settings, settings_.get())) {
    NOTREACHED();
    return OnError();
  }

  PrinterType printer_type = static_cast<PrinterType>(
      job_settings.FindIntKey(kSettingPrinterType).value());
  bool print_with_privet = printer_type == kPrivetPrinter;
  bool print_to_cloud = !!job_settings.FindKey(kSettingCloudPrintId);
  bool open_in_external_preview =
      !!job_settings.FindKey(kSettingOpenPDFInPreview);

  if (!open_in_external_preview &&
      (print_to_cloud || print_with_privet || printer_type == kPdfPrinter ||
       printer_type == kCloudPrinter || printer_type == kExtensionPrinter)) {
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
    return OK;
  }

  return UpdatePrinterSettings(
      open_in_external_preview,
      job_settings.FindBoolKey(kSettingShowSystemDialog).value_or(false),
      job_settings.FindIntKey(kSettingPreviewPageCount).value_or(0));
}

#if defined(OS_CHROMEOS)
PrintingContext::Result PrintingContext::UpdatePrintSettingsFromPOD(
    std::unique_ptr<PrintSettings> job_settings) {
  ResetSettings();
  settings_ = std::move(job_settings);

  return UpdatePrinterSettings(false /* external_preview */,
                               false /* show_system_dialog */,
                               0 /* page_count is only used on Android */);
}
#endif

}  // namespace printing
