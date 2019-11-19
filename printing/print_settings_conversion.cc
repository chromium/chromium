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

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/units.h"

namespace printing {

namespace {

// Note: If this code crashes, then the caller has passed in invalid |settings|.
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
                             base::DictionaryValue* job_settings) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger(kSettingMarginTop, margins.top);
  dict->SetInteger(kSettingMarginBottom, margins.bottom);
  dict->SetInteger(kSettingMarginLeft, margins.left);
  dict->SetInteger(kSettingMarginRight, margins.right);
  job_settings->Set(json_path, std::move(dict));
}

void SetSizeToJobSettings(const std::string& json_path,
                          const gfx::Size& size,
                          base::DictionaryValue* job_settings) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger("width", size.width());
  dict->SetInteger("height", size.height());
  job_settings->Set(json_path, std::move(dict));
}

void SetRectToJobSettings(const std::string& json_path,
                          const gfx::Rect& rect,
                          base::DictionaryValue* job_settings) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger("x", rect.x());
  dict->SetInteger("y", rect.y());
  dict->SetInteger("width", rect.width());
  dict->SetInteger("height", rect.height());
  job_settings->Set(json_path, std::move(dict));
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

      base::Optional<int> from = page_range.FindIntKey(kSettingPageRangeFrom);
      base::Optional<int> to = page_range.FindIntKey(kSettingPageRangeTo);
      if (!from.has_value() || !to.has_value())
        continue;

      // Page numbers are 1-based in the dictionary.
      // Page numbers are 0-based for the printing context.
      page_ranges.push_back(PageRange{from.value() - 1, to.value() - 1});
    }
  }
  return page_ranges;
}

bool PrintSettingsFromJobSettings(const base::Value& job_settings,
                                  PrintSettings* settings) {
  base::Optional<bool> display_header_footer =
      job_settings.FindBoolKey(kSettingHeaderFooterEnabled);
  if (!display_header_footer.has_value())
    return false;

  settings->set_display_header_footer(display_header_footer.value());
  if (settings->display_header_footer()) {
    const std::string* title =
        job_settings.FindStringKey(kSettingHeaderFooterTitle);
    const std::string* url =
        job_settings.FindStringKey(kSettingHeaderFooterURL);
    if (!title || !url)
      return false;

    settings->set_title(base::UTF8ToUTF16(*title));
    settings->set_url(base::UTF8ToUTF16(*url));
  }

  base::Optional<bool> backgrounds =
      job_settings.FindBoolKey(kSettingShouldPrintBackgrounds);
  base::Optional<bool> selection_only =
      job_settings.FindBoolKey(kSettingShouldPrintSelectionOnly);
  if (!backgrounds.has_value() || !selection_only.has_value())
    return false;

  settings->set_should_print_backgrounds(backgrounds.value());
  settings->set_selection_only(selection_only.value());

  PrintSettings::RequestedMedia requested_media;
  const base::Value* media_size_value = job_settings.FindKeyOfType(
      kSettingMediaSize, base::Value::Type::DICTIONARY);
  if (media_size_value) {
    base::Optional<int> width_microns =
        media_size_value->FindIntKey(kSettingMediaSizeWidthMicrons);
    base::Optional<int> height_microns =
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

  int margin_type =
      job_settings.FindIntKey(kSettingMarginsType).value_or(DEFAULT_MARGINS);
  if (margin_type != DEFAULT_MARGINS && margin_type != NO_MARGINS &&
      margin_type != CUSTOM_MARGINS && margin_type != PRINTABLE_AREA_MARGINS) {
    margin_type = DEFAULT_MARGINS;
  }
  settings->set_margin_type(static_cast<MarginType>(margin_type));

  if (margin_type == CUSTOM_MARGINS)
    settings->SetCustomMargins(GetCustomMarginsFromJobSettings(job_settings));

  settings->set_ranges(GetPageRangesFromJobSettings(job_settings));

  base::Optional<bool> collate = job_settings.FindBoolKey(kSettingCollate);
  base::Optional<int> copies = job_settings.FindIntKey(kSettingCopies);
  base::Optional<int> color = job_settings.FindIntKey(kSettingColor);
  base::Optional<int> duplex_mode = job_settings.FindIntKey(kSettingDuplexMode);
  base::Optional<bool> landscape = job_settings.FindBoolKey(kSettingLandscape);
  base::Optional<int> scale_factor =
      job_settings.FindIntKey(kSettingScaleFactor);
  base::Optional<bool> rasterize_pdf =
      job_settings.FindBoolKey(kSettingRasterizePdf);
  base::Optional<int> pages_per_sheet =
      job_settings.FindIntKey(kSettingPagesPerSheet);

  if (!collate.has_value() || !copies.has_value() || !color.has_value() ||
      !duplex_mode.has_value() || !landscape.has_value() ||
      !scale_factor.has_value() || !rasterize_pdf.has_value() ||
      !pages_per_sheet.has_value()) {
    return false;
  }
#if defined(OS_WIN)
  base::Optional<int> dpi_horizontal =
      job_settings.FindIntKey(kSettingDpiHorizontal);
  base::Optional<int> dpi_vertical =
      job_settings.FindIntKey(kSettingDpiVertical);
  if (!dpi_horizontal.has_value() || !dpi_vertical.has_value())
    return false;

  settings->set_dpi_xy(dpi_horizontal.value(), dpi_vertical.value());
#endif

  settings->set_collate(collate.value());
  settings->set_copies(copies.value());
  settings->SetOrientation(landscape.value());
  settings->set_device_name(
      base::UTF8ToUTF16(*job_settings.FindStringKey(kSettingDeviceName)));
  settings->set_duplex_mode(static_cast<DuplexMode>(duplex_mode.value()));
  settings->set_color(static_cast<ColorModel>(color.value()));
  settings->set_scale_factor(static_cast<double>(scale_factor.value()) / 100.0);
  settings->set_rasterize_pdf(rasterize_pdf.value());
  settings->set_pages_per_sheet(pages_per_sheet.value());
  base::Optional<bool> is_modifiable =
      job_settings.FindBoolKey(kSettingPreviewModifiable);
  if (is_modifiable.has_value()) {
    settings->set_is_modifiable(is_modifiable.value());
#if defined(OS_WIN)
    settings->set_print_text_with_gdi(is_modifiable.value());
#endif
  }

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

  const base::Value* advanced_settings =
      job_settings.FindDictKey(kSettingAdvancedSettings);
  if (advanced_settings) {
    for (const auto& item : advanced_settings->DictItems())
      settings->advanced_settings().emplace(item.first, item.second.Clone());
  }
#endif

  return true;
}

void PrintSettingsToJobSettingsDebug(const PrintSettings& settings,
                                     base::DictionaryValue* job_settings) {
  job_settings->SetBoolean(kSettingHeaderFooterEnabled,
                           settings.display_header_footer());
  job_settings->SetString(kSettingHeaderFooterTitle, settings.title());
  job_settings->SetString(kSettingHeaderFooterURL, settings.url());
  job_settings->SetBoolean(kSettingShouldPrintBackgrounds,
                           settings.should_print_backgrounds());
  job_settings->SetBoolean(kSettingShouldPrintSelectionOnly,
                           settings.selection_only());
  job_settings->SetInteger(kSettingMarginsType, settings.margin_type());
  if (!settings.ranges().empty()) {
    auto page_range_array = std::make_unique<base::ListValue>();
    for (size_t i = 0; i < settings.ranges().size(); ++i) {
      auto dict = std::make_unique<base::DictionaryValue>();
      dict->SetInteger(kSettingPageRangeFrom, settings.ranges()[i].from + 1);
      dict->SetInteger(kSettingPageRangeTo, settings.ranges()[i].to + 1);
      page_range_array->Append(std::move(dict));
    }
    job_settings->Set(kSettingPageRange, std::move(page_range_array));
  }

  job_settings->SetBoolean(kSettingCollate, settings.collate());
  job_settings->SetInteger(kSettingCopies, settings.copies());
  job_settings->SetInteger(kSettingColor, settings.color());
  job_settings->SetInteger(kSettingDuplexMode, settings.duplex_mode());
  job_settings->SetBoolean(kSettingLandscape, settings.landscape());
  job_settings->SetString(kSettingDeviceName, settings.device_name());
  job_settings->SetInteger(kSettingPagesPerSheet, settings.pages_per_sheet());

  // Following values are not read form JSON by InitSettings, so do not have
  // common public constants. So just serialize in "debug" section.
  auto debug = std::make_unique<base::DictionaryValue>();
  debug->SetInteger("dpi", settings.dpi());
  debug->SetInteger("deviceUnitsPerInch", settings.device_units_per_inch());
  debug->SetBoolean("support_alpha_blend", settings.should_print_backgrounds());
  debug->SetString("media_vendor_id", settings.requested_media().vendor_id);
  SetSizeToJobSettings("media_size", settings.requested_media().size_microns,
                       debug.get());
  SetMarginsToJobSettings("requested_custom_margins_in_points",
                          settings.requested_custom_margins_in_points(),
                          debug.get());
  const PageSetup& page_setup = settings.page_setup_device_units();
  SetMarginsToJobSettings("effective_margins", page_setup.effective_margins(),
                          debug.get());
  SetSizeToJobSettings("physical_size", page_setup.physical_size(),
                       debug.get());
  SetRectToJobSettings("overlay_area", page_setup.overlay_area(), debug.get());
  SetRectToJobSettings("content_area", page_setup.content_area(), debug.get());
  SetRectToJobSettings("printable_area", page_setup.printable_area(),
                       debug.get());
  job_settings->Set("debug", std::move(debug));
}

}  // namespace printing
