// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_conversion.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/units.h"

namespace printing {

namespace {

// Note: If this code crashes, then the caller has passed in invalid `settings`.
// Fix the caller, instead of trying to avoid the crash here.
PageMargins GetCustomMarginsFromJobSettings(const base::Value& settings) {
  PageMargins margins_in_points;
  const base::Value* custom_margins = settings.FindKey(kSettingMarginsCustom);
  margins_in_points.top = custom_margins->FindIntKey(kSettingMarginTop).value();
  margins_in_points.bottom =
      custom_margins->FindIntKey(kSettingMarginBottom).value();
  margins_in_points.left =
      custom_margins->FindIntKey(kSettingMarginLeft).value();
  margins_in_points.right =
      custom_margins->FindIntKey(kSettingMarginRight).value();
  return margins_in_points;
}

void SetMarginsToJobSettings(const std::string& json_path,
                             const PageMargins& margins,
                             base::Value& job_settings) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey(kSettingMarginTop, margins.top);
  dict.SetIntKey(kSettingMarginBottom, margins.bottom);
  dict.SetIntKey(kSettingMarginLeft, margins.left);
  dict.SetIntKey(kSettingMarginRight, margins.right);
  job_settings.SetKey(json_path, std::move(dict));
}

void SetSizeToJobSettings(const std::string& json_path,
                          const gfx::Size& size,
                          base::Value& job_settings) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("width", size.width());
  dict.SetIntKey("height", size.height());
  job_settings.SetKey(json_path, std::move(dict));
}

void SetRectToJobSettings(const std::string& json_path,
                          const gfx::Rect& rect,
                          base::Value& job_settings) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("x", rect.x());
  dict.SetIntKey("y", rect.y());
  dict.SetIntKey("width", rect.width());
  dict.SetIntKey("height", rect.height());
  job_settings.SetKey(json_path, std::move(dict));
}

}  // namespace

PageRanges GetPageRangesFromJobSettings(const base::Value& job_settings) {
  PageRanges page_ranges;
  const base::Value* page_range_array =
      job_settings.FindListKey(kSettingPageRange);
  if (page_range_array) {
    for (const base::Value& page_range : page_range_array->GetList()) {
      if (!page_range.is_dict())
        continue;

      absl::optional<int> from = page_range.FindIntKey(kSettingPageRangeFrom);
      absl::optional<int> to = page_range.FindIntKey(kSettingPageRangeTo);
      if (!from.has_value() || !to.has_value())
        continue;

      // Page numbers are 1-based in the dictionary.
      // Page numbers are 0-based for the printing context.
      page_ranges.push_back(PageRange{static_cast<uint32_t>(from.value() - 1),
                                      static_cast<uint32_t>(to.value() - 1)});
    }
  }
  return page_ranges;
}

std::unique_ptr<PrintSettings> PrintSettingsFromJobSettings(
    const base::Value& job_settings) {
  auto settings = std::make_unique<PrintSettings>();
  absl::optional<bool> display_header_footer =
      job_settings.FindBoolKey(kSettingHeaderFooterEnabled);
  if (!display_header_footer.has_value())
    return nullptr;

  settings->set_display_header_footer(display_header_footer.value());
  if (settings->display_header_footer()) {
    const std::string* title =
        job_settings.FindStringKey(kSettingHeaderFooterTitle);
    const std::string* url =
        job_settings.FindStringKey(kSettingHeaderFooterURL);
    if (!title || !url)
      return nullptr;

    settings->set_title(base::UTF8ToUTF16(*title));
    settings->set_url(base::UTF8ToUTF16(*url));
  }

  absl::optional<bool> backgrounds =
      job_settings.FindBoolKey(kSettingShouldPrintBackgrounds);
  absl::optional<bool> selection_only =
      job_settings.FindBoolKey(kSettingShouldPrintSelectionOnly);
  if (!backgrounds.has_value() || !selection_only.has_value())
    return nullptr;

  settings->set_should_print_backgrounds(backgrounds.value());
  settings->set_selection_only(selection_only.value());

  PrintSettings::RequestedMedia requested_media;
  const base::Value* media_size_value = job_settings.FindKeyOfType(
      kSettingMediaSize, base::Value::Type::DICTIONARY);
  if (media_size_value) {
    absl::optional<int> width_microns =
        media_size_value->FindIntKey(kSettingMediaSizeWidthMicrons);
    absl::optional<int> height_microns =
        media_size_value->FindIntKey(kSettingMediaSizeHeightMicrons);
    if (width_microns.has_value() && height_microns.has_value()) {
      requested_media.size_microns =
          gfx::Size(width_microns.value(), height_microns.value());
    }

    const std::string* vendor_id =
        media_size_value->FindStringKey(kSettingMediaSizeVendorId);
    if (vendor_id && !vendor_id->empty())
      requested_media.vendor_id = *vendor_id;
  }
  settings->set_requested_media(requested_media);

  mojom::MarginType margin_type = static_cast<mojom::MarginType>(
      job_settings.FindIntKey(kSettingMarginsType)
          .value_or(static_cast<int>(mojom::MarginType::kDefaultMargins)));
  if (margin_type != mojom::MarginType::kDefaultMargins &&
      margin_type != mojom::MarginType::kNoMargins &&
      margin_type != mojom::MarginType::kCustomMargins &&
      margin_type != mojom::MarginType::kPrintableAreaMargins) {
    margin_type = mojom::MarginType::kDefaultMargins;
  }
  settings->set_margin_type(margin_type);

  if (margin_type == mojom::MarginType::kCustomMargins)
    settings->SetCustomMargins(GetCustomMarginsFromJobSettings(job_settings));

  settings->set_ranges(GetPageRangesFromJobSettings(job_settings));

  absl::optional<bool> collate = job_settings.FindBoolKey(kSettingCollate);
  absl::optional<int> copies = job_settings.FindIntKey(kSettingCopies);
  absl::optional<int> color = job_settings.FindIntKey(kSettingColor);
  absl::optional<int> duplex_mode = job_settings.FindIntKey(kSettingDuplexMode);
  absl::optional<bool> landscape = job_settings.FindBoolKey(kSettingLandscape);
  absl::optional<int> scale_factor =
      job_settings.FindIntKey(kSettingScaleFactor);
  absl::optional<bool> rasterize_pdf =
      job_settings.FindBoolKey(kSettingRasterizePdf);
  absl::optional<int> pages_per_sheet =
      job_settings.FindIntKey(kSettingPagesPerSheet);

  if (!collate.has_value() || !copies.has_value() || !color.has_value() ||
      !duplex_mode.has_value() || !landscape.has_value() ||
      !scale_factor.has_value() || !rasterize_pdf.has_value() ||
      !pages_per_sheet.has_value()) {
    return nullptr;
  }

  absl::optional<int> dpi_horizontal =
      job_settings.FindIntKey(kSettingDpiHorizontal);
  absl::optional<int> dpi_vertical =
      job_settings.FindIntKey(kSettingDpiVertical);
  if (!dpi_horizontal.has_value() || !dpi_vertical.has_value())
    return nullptr;
  settings->set_dpi_xy(dpi_horizontal.value(), dpi_vertical.value());

  absl::optional<int> rasterize_pdf_dpi =
      job_settings.FindIntKey(kSettingRasterizePdfDpi);
  if (rasterize_pdf_dpi.has_value())
    settings->set_rasterize_pdf_dpi(rasterize_pdf_dpi.value());

  settings->set_collate(collate.value());
  settings->set_copies(copies.value());
  settings->SetOrientation(landscape.value());
  settings->set_device_name(
      base::UTF8ToUTF16(*job_settings.FindStringKey(kSettingDeviceName)));
  settings->set_duplex_mode(
      static_cast<mojom::DuplexMode>(duplex_mode.value()));
  settings->set_color(static_cast<mojom::ColorModel>(color.value()));
  settings->set_scale_factor(static_cast<double>(scale_factor.value()) / 100.0);
  settings->set_rasterize_pdf(rasterize_pdf.value());
  settings->set_pages_per_sheet(pages_per_sheet.value());
  absl::optional<bool> is_modifiable =
      job_settings.FindBoolKey(kSettingPreviewModifiable);
  if (is_modifiable.has_value()) {
    settings->set_is_modifiable(is_modifiable.value());
  }

#if defined(OS_CHROMEOS) || (defined(OS_LINUX) && defined(USE_CUPS))
  const base::Value* advanced_settings =
      job_settings.FindDictKey(kSettingAdvancedSettings);
  if (advanced_settings) {
    for (const auto item : advanced_settings->DictItems()) {
      static constexpr auto kNonJobAttributes =
          base::MakeFixedFlatSet<base::StringPiece>(
              {"printer-info", "printer-make-and-model", "system_driverinfo"});
      if (!base::Contains(kNonJobAttributes, item.first))
        settings->advanced_settings().emplace(item.first, item.second.Clone());
    }
  }
#endif  // defined(OS_CHROMEOS) || (defined(OS_LINUX) && defined(USE_CUPS))

#if defined(OS_CHROMEOS)
  bool send_user_info =
      job_settings.FindBoolKey(kSettingSendUserInfo).value_or(false);
  settings->set_send_user_info(send_user_info);
  if (send_user_info) {
    const std::string* username = job_settings.FindStringKey(kSettingUsername);
    if (username)
      settings->set_username(*username);
  }

  const std::string* pin_value = job_settings.FindStringKey(kSettingPinValue);
  if (pin_value)
    settings->set_pin_value(*pin_value);
#endif  // defined(OS_CHROMEOS)

  return settings;
}

base::Value PrintSettingsToJobSettingsDebug(const PrintSettings& settings) {
  base::Value job_settings(base::Value::Type::DICTIONARY);

  job_settings.SetBoolKey(kSettingHeaderFooterEnabled,
                          settings.display_header_footer());
  job_settings.SetStringKey(kSettingHeaderFooterTitle, settings.title());
  job_settings.SetStringKey(kSettingHeaderFooterURL, settings.url());
  job_settings.SetBoolKey(kSettingShouldPrintBackgrounds,
                          settings.should_print_backgrounds());
  job_settings.SetBoolKey(kSettingShouldPrintSelectionOnly,
                          settings.selection_only());
  job_settings.SetIntKey(kSettingMarginsType,
                         static_cast<int>(settings.margin_type()));
  if (!settings.ranges().empty()) {
    base::ListValue page_range_array;
    for (const auto& range : settings.ranges()) {
      auto dict = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
      dict->SetIntKey(kSettingPageRangeFrom, range.from + 1);
      dict->SetIntKey(kSettingPageRangeTo, range.to + 1);
      page_range_array.Append(std::move(dict));
    }
    job_settings.SetKey(kSettingPageRange, std::move(page_range_array));
  }

  job_settings.SetBoolKey(kSettingCollate, settings.collate());
  job_settings.SetIntKey(kSettingCopies, settings.copies());
  job_settings.SetIntKey(kSettingColor, static_cast<int>(settings.color()));
  job_settings.SetIntKey(kSettingDuplexMode,
                         static_cast<int>(settings.duplex_mode()));
  job_settings.SetBoolKey(kSettingLandscape, settings.landscape());
  job_settings.SetStringKey(kSettingDeviceName, settings.device_name());
  job_settings.SetIntKey(kSettingDpiHorizontal, settings.dpi_horizontal());
  job_settings.SetIntKey(kSettingDpiVertical, settings.dpi_vertical());
  job_settings.SetIntKey(
      kSettingScaleFactor,
      static_cast<int>((settings.scale_factor() * 100.0) + 0.5));
  job_settings.SetBoolKey(kSettingRasterizePdf, settings.rasterize_pdf());
  job_settings.SetIntKey(kSettingPagesPerSheet, settings.pages_per_sheet());

  // Following values are not read form JSON by InitSettings, so do not have
  // common public constants. So just serialize in "debug" section.
  base::Value debug(base::Value::Type::DICTIONARY);
  debug.SetIntKey("dpi", settings.dpi());
  debug.SetIntKey("deviceUnitsPerInch", settings.device_units_per_inch());
  debug.SetBoolKey("support_alpha_blend", settings.should_print_backgrounds());
  debug.SetStringKey("media_vendor_id", settings.requested_media().vendor_id);
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
  job_settings.SetKey("debug", std::move(debug));

  return job_settings;
}

}  // namespace printing
