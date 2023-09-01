// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_MOJOM_PRINTING_CONTEXT_MOJOM_TRAITS_H_
#define PRINTING_MOJOM_PRINTING_CONTEXT_MOJOM_TRAITS_H_

#include <string>

#include "build/build_config.h"
#include "printing/mojom/print.mojom.h"
#include "printing/mojom/printing_context.mojom-shared.h"
#include "printing/page_setup.h"
#include "printing/print_settings.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
#include "base/values.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"
#endif

namespace mojo {

template <>
struct StructTraits<printing::mojom::PageMarginsDataView,
                    printing::PageMargins> {
  static int32_t header(const printing::PageMargins& m) { return m.header; }
  static int32_t footer(const printing::PageMargins& m) { return m.footer; }
  static int32_t left(const printing::PageMargins& m) { return m.left; }
  static int32_t right(const printing::PageMargins& m) { return m.right; }
  static int32_t top(const printing::PageMargins& m) { return m.top; }
  static int32_t bottom(const printing::PageMargins& m) { return m.bottom; }

  static bool Read(printing::mojom::PageMarginsDataView data,
                   printing::PageMargins* out);
};

template <>
struct StructTraits<printing::mojom::PageSetupDataView, printing::PageSetup> {
  static const gfx::Size& physical_size(const printing::PageSetup& s) {
    return s.physical_size();
  }
  static const gfx::Rect& printable_area(const printing::PageSetup& s) {
    return s.printable_area();
  }
  static const gfx::Rect& overlay_area(const printing::PageSetup& s) {
    return s.overlay_area();
  }
  static const gfx::Rect& content_area(const printing::PageSetup& s) {
    return s.content_area();
  }
  static const printing::PageMargins& effective_margins(
      const printing::PageSetup& s) {
    return s.effective_margins();
  }
  static const printing::PageMargins& requested_margins(
      const printing::PageSetup& s) {
    return s.requested_margins();
  }
  static bool forced_margins(const printing::PageSetup& s) {
    return s.forced_margins();
  }
  static int32_t text_height(const printing::PageSetup& s) {
    return s.text_height();
  }

  static bool Read(printing::mojom::PageSetupDataView data,
                   printing::PageSetup* out);
};

template <>
struct StructTraits<printing::mojom::RequestedMediaDataView,
                    printing::PrintSettings::RequestedMedia> {
  static const gfx::Size& size_microns(
      const printing::PrintSettings::RequestedMedia& r) {
    return r.size_microns;
  }
  static const std::string& vendor_id(
      const printing::PrintSettings::RequestedMedia& r) {
    return r.vendor_id;
  }

  static bool Read(printing::mojom::RequestedMediaDataView data,
                   printing::PrintSettings::RequestedMedia* out);
};

template <>
struct StructTraits<printing::mojom::PrintSettingsDataView,
                    printing::PrintSettings> {
  static const printing::PageRanges& ranges(const printing::PrintSettings& s) {
    return s.ranges();
  }
  static bool selection_only(const printing::PrintSettings& s) {
    return s.selection_only();
  }
  static printing::mojom::MarginType margin_type(
      const printing::PrintSettings& s) {
    return s.margin_type();
  }
  static const std::u16string& title(const printing::PrintSettings& s) {
    return s.title();
  }
  static const std::u16string& url(const printing::PrintSettings& s) {
    return s.url();
  }
  static bool display_header_footer(const printing::PrintSettings& s) {
    return s.display_header_footer();
  }
  static bool should_print_backgrounds(const printing::PrintSettings& s) {
    return s.should_print_backgrounds();
  }
  static bool collate(const printing::PrintSettings& s) { return s.collate(); }
  static printing::mojom::ColorModel color(const printing::PrintSettings& s) {
    return s.color();
  }
  static int32_t copies(const printing::PrintSettings& s) { return s.copies(); }
  static printing::mojom::DuplexMode duplex_mode(
      const printing::PrintSettings& s) {
    return s.duplex_mode();
  }
  static const std::u16string& device_name(const printing::PrintSettings& s) {
    return s.device_name();
  }
  static const printing::PrintSettings::RequestedMedia& requested_media(
      const printing::PrintSettings& s) {
    return s.requested_media();
  }
  static const printing::PageSetup& page_setup_device_units(
      const printing::PrintSettings& s) {
    return s.page_setup_device_units();
  }
  static const std::string& media_type(const printing::PrintSettings& s) {
    return s.media_type();
  }
  static bool borderless(const printing::PrintSettings& s) {
    return s.borderless();
  }
  static const gfx::Size& dpi(const printing::PrintSettings& s) {
    return s.dpi_size();
  }
  static double scale_factor(const printing::PrintSettings& s) {
    return s.scale_factor();
  }
  static bool rasterize_pdf(const printing::PrintSettings& s) {
    return s.rasterize_pdf();
  }
  static bool landscape(const printing::PrintSettings& s) {
    return s.landscape();
  }

#if BUILDFLAG(IS_WIN)
  static printing::mojom::PrinterLanguageType printer_language_type(
      const printing::PrintSettings& s) {
    return s.printer_language_type();
  }
#endif  // BUILDFLAG(IS_WIN)

  static bool is_modifiable(const printing::PrintSettings& s) {
    return s.is_modifiable();
  }
  static const printing::PageMargins& requested_custom_margins_in_points(
      const printing::PrintSettings& s) {
    return s.requested_custom_margins_in_points();
  }
  static int32_t pages_per_sheet(const printing::PrintSettings& s) {
    return s.pages_per_sheet();
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static const printing::PrintSettings::AdvancedSettings& advanced_settings(
      const printing::PrintSettings& s) {
    return s.advanced_settings();
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  static bool send_user_info(const printing::PrintSettings& s) {
    return s.send_user_info();
  }
  static const std::string& username(const printing::PrintSettings& s) {
    return s.username();
  }
  static const std::string& pin_value(const printing::PrintSettings& s) {
    return s.pin_value();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  static const base::Value::Dict& system_print_dialog_data(
      const printing::PrintSettings& s) {
    return s.system_print_dialog_data();
  }
#endif

  static bool Read(printing::mojom::PrintSettingsDataView data,
                   printing::PrintSettings* out);
};

}  // namespace mojo

#endif  // PRINTING_MOJOM_PRINTING_CONTEXT_MOJOM_TRAITS_H_
