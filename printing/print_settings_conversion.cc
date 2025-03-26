// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_conversion.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/to_value_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "print_settings_conversion_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace printing {

namespace {

// Note: If this code crashes, then the caller has passed in invalid `settings`.
// Fix the caller, instead of trying to avoid the crash here.
PageMargins GetCustomMarginsFromJobSettings(const base::Value::Dict& settings) {
  PageMargins margins_in_points;
  const base::Value::Dict* custom_margins =
      settings.FindDict(kSettingMarginsCustom);
  margins_in_points.top = custom_margins->FindInt(kSettingMarginTop).value();
  margins_in_points.bottom =
      custom_margins->FindInt(kSettingMarginBottom).value();
  margins_in_points.left = custom_margins->FindInt(kSettingMarginLeft).value();
  margins_in_points.right =
      custom_margins->FindInt(kSettingMarginRight).value();
  return margins_in_points;
}

void SetMarginsToJobSettings(const std::string& json_path,
                             const PageMargins& margins,
                             base::Value::Dict& job_settings) {
  job_settings.Set(json_path, base::Value::Dict()
                                  .Set(kSettingMarginTop, margins.top)
                                  .Set(kSettingMarginBottom, margins.bottom)
                                  .Set(kSettingMarginLeft, margins.left)
                                  .Set(kSettingMarginRight, margins.right));
}

void SetSizeToJobSettings(const std::string& json_path,
                          const gfx::Size& size,
                          base::Value::Dict& job_settings) {
  job_settings.Set(json_path, base::Value::Dict()
                                  .Set("width", size.width())
                                  .Set("height", size.height()));
}

void SetRectToJobSettings(const std::string& json_path,
                          const gfx::Rect& rect,
                          base::Value::Dict& job_settings) {
  job_settings.Set(json_path, base::Value::Dict()
                                  .Set("x", rect.x())
                                  .Set("y", rect.y())
                                  .Set("width", rect.width())
                                  .Set("height", rect.height()));
}

void SetPrintableAreaIfValid(PrintSettings& settings,
                             const gfx::Size& size_microns,
                             const base::Value::Dict& media_size) {
  std::optional<int> left_microns =
      media_size.FindInt(kSettingsImageableAreaLeftMicrons);
  std::optional<int> bottom_microns =
      media_size.FindInt(kSettingsImageableAreaBottomMicrons);
  std::optional<int> right_microns =
      media_size.FindInt(kSettingsImageableAreaRightMicrons);
  std::optional<int> top_microns =
      media_size.FindInt(kSettingsImageableAreaTopMicrons);
  if (!bottom_microns.has_value() || !left_microns.has_value() ||
      !right_microns.has_value() || !top_microns.has_value()) {
    return;
  }

  // Scale the page size and printable area to device units.
  // Blink doesn't support different dpi settings in X and Y axis. Because of
  // this, printers with non-square pixels still scale page size and printable
  // area using device_units_per_inch() instead of their respective dimensions
  // in device_units_per_inch_size().
  float scale =
      static_cast<float>(settings.device_units_per_inch()) / kMicronsPerInch;
  gfx::Size page_size = gfx::ScaleToRoundedSize(size_microns, scale);
  // Flip the y-axis since the imageable area origin is at the bottom-left,
  // while the gfx::Rect origin is at the top-left.
  gfx::Rect printable_area = gfx::ScaleToRoundedRect(
      {left_microns.value(), size_microns.height() - top_microns.value(),
       right_microns.value() - left_microns.value(),
       top_microns.value() - bottom_microns.value()},
      scale);
  // Sanity check that the printable area makes sense.
  if (printable_area.IsEmpty() ||
      !gfx::Rect(page_size).Contains(printable_area)) {
    return;
  }
  settings.SetPrinterPrintableArea(page_size, printable_area,
                                   /*landscape_needs_flip=*/true);
}

}  // namespace

PageRanges GetPageRangesFromJobSettings(const base::Value::Dict& job_settings) {
  PageRanges page_ranges;
  const base::Value::List* page_range_array =
      job_settings.FindList(kSettingPageRange);
  if (!page_range_array) {
    return page_ranges;
  }

  for (const base::Value& page_range : *page_range_array) {
    if (!page_range.is_dict()) {
      continue;
    }

    const auto& dict = page_range.GetDict();
    std::optional<int> from = dict.FindInt(kSettingPageRangeFrom);
    std::optional<int> to = dict.FindInt(kSettingPageRangeTo);
    if (!from.has_value() || !to.has_value()) {
      continue;
    }

    // Page numbers are 1-based in the dictionary.
    // Page numbers are 0-based for the printing context.
    page_ranges.push_back(PageRange{static_cast<uint32_t>(from.value() - 1),
                                    static_cast<uint32_t>(to.value() - 1)});
  }
  return page_ranges;
}

std::unique_ptr<PrintSettings> PrintSettingsFromJobSettings(
    const base::Value::Dict& job_settings) {
  auto settings = std::make_unique<PrintSettings>();
  std::optional<bool> display_header_footer =
      job_settings.FindBool(kSettingHeaderFooterEnabled);
  if (!display_header_footer.has_value()) {
    return nullptr;
  }

  settings->set_display_header_footer(display_header_footer.value());
  if (settings->display_header_footer()) {
    const std::string* title =
        job_settings.FindString(kSettingHeaderFooterTitle);
    const std::string* url = job_settings.FindString(kSettingHeaderFooterURL);
    if (!title || !url) {
      return nullptr;
    }

    settings->set_title(base::UTF8ToUTF16(*title));
    settings->set_url(base::UTF8ToUTF16(*url));
  }

  std::optional<bool> backgrounds =
      job_settings.FindBool(kSettingShouldPrintBackgrounds);
  std::optional<bool> selection_only =
      job_settings.FindBool(kSettingShouldPrintSelectionOnly);
  if (!backgrounds.has_value() || !selection_only.has_value()) {
    return nullptr;
  }

  settings->set_should_print_backgrounds(backgrounds.value());
  settings->set_selection_only(selection_only.value());

  std::optional<bool> collate = job_settings.FindBool(kSettingCollate);
  std::optional<int> copies = job_settings.FindInt(kSettingCopies);
  std::optional<int> color = job_settings.FindInt(kSettingColor);
  std::optional<int> duplex_mode = job_settings.FindInt(kSettingDuplexMode);
  std::optional<bool> landscape = job_settings.FindBool(kSettingLandscape);
  const std::string* device_name = job_settings.FindString(kSettingDeviceName);
  std::optional<int> scale_factor = job_settings.FindInt(kSettingScaleFactor);
  std::optional<bool> rasterize_pdf =
      job_settings.FindBool(kSettingRasterizePdf);
  std::optional<int> pages_per_sheet =
      job_settings.FindInt(kSettingPagesPerSheet);
  if (!collate.has_value() || !copies.has_value() || !color.has_value() ||
      !duplex_mode.has_value() || !landscape.has_value() || !device_name ||
      !scale_factor.has_value() || !rasterize_pdf.has_value() ||
      !pages_per_sheet.has_value()) {
    return nullptr;
  }
  settings->set_collate(collate.value());
  settings->set_copies(copies.value());
  settings->SetOrientation(landscape.value());
  settings->set_device_name(base::UTF8ToUTF16(*device_name));
  settings->set_duplex_mode(
      static_cast<mojom::DuplexMode>(duplex_mode.value()));
  settings->set_color(static_cast<mojom::ColorModel>(color.value()));
  settings->set_scale_factor(static_cast<double>(scale_factor.value()) / 100.0);
  settings->set_rasterize_pdf(rasterize_pdf.value());
  settings->set_pages_per_sheet(pages_per_sheet.value());

  std::optional<int> dpi_horizontal =
      job_settings.FindInt(kSettingDpiHorizontal);
  std::optional<int> dpi_vertical = job_settings.FindInt(kSettingDpiVertical);
  if (!dpi_horizontal.has_value() || !dpi_vertical.has_value()) {
    return nullptr;
  }

  settings->set_dpi_xy(dpi_horizontal.value(), dpi_vertical.value());

  std::optional<int> rasterize_pdf_dpi =
      job_settings.FindInt(kSettingRasterizePdfDpi);
  if (rasterize_pdf_dpi.has_value()) {
    settings->set_rasterize_pdf_dpi(rasterize_pdf_dpi.value());
  }

  // Set margin type before setting printable area. `SetPrintableAreaIfValid()`
  // calls `PrintSettings::SetPrinterPrintableArea()`, which requires the margin
  // type to be set first.
  mojom::MarginType margin_type = static_cast<mojom::MarginType>(
      job_settings.FindInt(kSettingMarginsType)
          .value_or(static_cast<int>(mojom::MarginType::kDefaultMargins)));
  if (margin_type < mojom::MarginType::kMinValue ||
      margin_type > mojom::MarginType::kMaxValue) {
    margin_type = mojom::MarginType::kDefaultMargins;
  }
  settings->set_margin_type(margin_type);

  if (margin_type == mojom::MarginType::kCustomMargins) {
    settings->SetCustomMargins(GetCustomMarginsFromJobSettings(job_settings));
  }

  PrintSettings::RequestedMedia requested_media;
  const base::Value::Dict* media_size_value =
      job_settings.FindDict(kSettingMediaSize);
  if (media_size_value) {
    std::optional<int> width_microns =
        media_size_value->FindInt(kSettingMediaSizeWidthMicrons);
    std::optional<int> height_microns =
        media_size_value->FindInt(kSettingMediaSizeHeightMicrons);
    if (width_microns.has_value() && height_microns.has_value()) {
      requested_media.size_microns =
          gfx::Size(width_microns.value(), height_microns.value());
      SetPrintableAreaIfValid(*settings, requested_media.size_microns,
                              *media_size_value);
    }

    const std::string* vendor_id =
        media_size_value->FindString(kSettingMediaSizeVendorId);
    if (vendor_id && !vendor_id->empty()) {
      requested_media.vendor_id = *vendor_id;
    }
  }
  settings->set_requested_media(requested_media);

  std::optional<bool> borderless = job_settings.FindBool(kSettingBorderless);
  if (borderless.has_value()) {
    settings->set_borderless(borderless.value());
  }

  const std::string* media_type = job_settings.FindString(kSettingMediaType);
  if (media_type && !media_type->empty()) {
    settings->set_media_type(*media_type);
  }

  settings->set_ranges(GetPageRangesFromJobSettings(job_settings));

  std::optional<bool> is_modifiable =
      job_settings.FindBool(kSettingPreviewModifiable);
  if (is_modifiable.has_value()) {
    settings->set_is_modifiable(is_modifiable.value());
  }

#if BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS))
  const base::Value::Dict* advanced_settings =
      job_settings.FindDict(kSettingAdvancedSettings);
  if (advanced_settings) {
    for (const auto item : *advanced_settings) {
      static constexpr auto kNonJobAttributes =
          base::MakeFixedFlatSet<std::string_view>(
              {"printer-info", "printer-make-and-model", "system_driverinfo"});
      if (!base::Contains(kNonJobAttributes, item.first)) {
        settings->advanced_settings().emplace(item.first, item.second.Clone());
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) &&
        // BUILDFLAG(USE_CUPS))

#if BUILDFLAG(IS_CHROMEOS)
  bool send_user_info =
      job_settings.FindBool(kSettingSendUserInfo).value_or(false);
  settings->set_send_user_info(send_user_info);
  if (send_user_info) {
    const std::string* username = job_settings.FindString(kSettingUsername);
    if (username) {
      settings->set_username(*username);
    }
  }

  const std::string* oauth_token =
      job_settings.FindString(kSettingChromeOSAccessOAuthToken);
  if (oauth_token) {
    settings->set_oauth_token(*oauth_token);
  }

  const std::string* pin_value = job_settings.FindString(kSettingPinValue);
  if (pin_value) {
    settings->set_pin_value(*pin_value);
  }

  const base::Value::List* client_info_list =
      job_settings.FindList(kSettingIppClientInfo);
  if (client_info_list) {
    settings->set_client_infos(
        ConvertJobSettingToClientInfo(*client_info_list));
  }

  settings->set_printer_manually_selected(
      job_settings.FindBool(kSettingPrinterManuallySelected).value_or(false));

  std::optional<int> reason = job_settings.FindInt(kSettingPrinterStatusReason);
  if (reason.has_value()) {
    settings->set_printer_status_reason(
        static_cast<crosapi::mojom::StatusReason::Reason>(reason.value()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return settings;
}

base::Value::Dict PrintSettingsToJobSettingsDebug(
    const PrintSettings& settings) {
  auto job_settings =
      base::Value::Dict()
          .Set(kSettingHeaderFooterEnabled, settings.display_header_footer())
          .Set(kSettingHeaderFooterTitle, settings.title())
          .Set(kSettingHeaderFooterURL, settings.url())
          .Set(kSettingShouldPrintBackgrounds,
               settings.should_print_backgrounds())
          .Set(kSettingShouldPrintSelectionOnly, settings.selection_only())
          .Set(kSettingMarginsType, static_cast<int>(settings.margin_type()))
          .Set(kSettingCollate, settings.collate())
          .Set(kSettingCopies, settings.copies())
          .Set(kSettingColor, static_cast<int>(settings.color()))
          .Set(kSettingDuplexMode, static_cast<int>(settings.duplex_mode()))
          .Set(kSettingLandscape, settings.landscape())
          .Set(kSettingDeviceName, settings.device_name())
          .Set(kSettingDpiHorizontal, settings.dpi_horizontal())
          .Set(kSettingDpiVertical, settings.dpi_vertical())
          .Set(kSettingScaleFactor,
               static_cast<int>((settings.scale_factor() * 100.0) + 0.5))
          .Set(kSettingRasterizePdf, settings.rasterize_pdf())
          .Set(kSettingPagesPerSheet, settings.pages_per_sheet());

  if (!settings.ranges().empty()) {
    job_settings.Set(
        kSettingPageRange,
        base::ToValueList(settings.ranges(), [](const auto& range) {
          return base::Value::Dict()
              .Set(kSettingPageRangeFrom, static_cast<int>(range.from + 1))
              .Set(kSettingPageRangeTo, static_cast<int>(range.to + 1));
        }));
  }

  // Following values are not read form JSON by InitSettings, so do not have
  // common public constants. So just serialize in "debug" section.
  auto debug =
      base::Value::Dict()
          .Set("dpi", settings.dpi())
          .Set("deviceUnitsPerInch", settings.device_units_per_inch())
          .Set("support_alpha_blend", settings.should_print_backgrounds())
          .Set("media_vendor_id", settings.requested_media().vendor_id);

  SetSizeToJobSettings("media_size", settings.requested_media().size_microns,
                       debug);
  SetMarginsToJobSettings("requested_custom_margins_in_points",
                          settings.requested_custom_margins_in_points(), debug);
  const PageSetup& page_setup = settings.page_setup_device_units();
  SetMarginsToJobSettings("effective_margins", page_setup.effective_margins(),
                          debug);
  SetSizeToJobSettings("physical_size", page_setup.physical_size(), debug);
  SetRectToJobSettings("overlay_area", page_setup.overlay_area(), debug);
  SetRectToJobSettings("content_area", page_setup.content_area(), debug);
  SetRectToJobSettings("printable_area", page_setup.printable_area(), debug);
  job_settings.Set("debug", std::move(debug));

#if BUILDFLAG(IS_CHROMEOS)
  job_settings.Set(kSettingIppClientInfo,
                   ConvertClientInfoToJobSetting(settings.client_infos()));
#endif  // BUILDFLAG(IS_CHROMEOS)

  return job_settings;
}

}  // namespace printing
