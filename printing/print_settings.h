// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_SETTINGS_H_
#define PRINTING_PRINT_SETTINGS_H_

#include <algorithm>
#include <string>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"
#include "printing/page_setup.h"
#include "printing/print_job_constants.h"
#include "printing/printing_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <map>

#include "base/values.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

namespace printing {

// Convert from |color_mode| into a |color_model|.  An invalid |color_mode|
// will give a result of |mojom::ColorModel::kUnknownColorModel|.
PRINTING_EXPORT mojom::ColorModel ColorModeToColorModel(int color_mode);

// Returns true if |color_model| is color and false if it is B&W.  Callers
// are not supposed to pass in |mojom::ColorModel::kUnknownColorModel|, but
// if they do then the result will be base::nullopt.
PRINTING_EXPORT base::Optional<bool> IsColorModelSelected(
    mojom::ColorModel color_model);

#if defined(USE_CUPS)
// Get the color model setting name and value for the |color_model|.
PRINTING_EXPORT void GetColorModelForModel(mojom::ColorModel color_model,
                                           std::string* color_setting_name,
                                           std::string* color_value);

#if defined(OS_MAC) || defined(OS_CHROMEOS)
// Convert from |color_model| to a print-color-mode value from PWG 5100.13.
PRINTING_EXPORT std::string GetIppColorModelForModel(
    mojom::ColorModel color_model);
#endif
#endif  // defined(USE_CUPS)

// Inform the printing system that it may embed this user-agent string
// in its output's metadata.
PRINTING_EXPORT void SetAgent(const std::string& user_agent);
PRINTING_EXPORT const std::string& GetAgent();

class PRINTING_EXPORT PrintSettings {
 public:
#if defined(OS_WIN)
  enum PrinterType {
    TYPE_NONE = 0,
    TYPE_TEXTONLY,
    TYPE_XPS,
    TYPE_POSTSCRIPT_LEVEL2,
    TYPE_POSTSCRIPT_LEVEL3
  };
#endif

  // Media properties requested by the user. Default instance represents
  // default media selection.
  struct RequestedMedia {
    // Size of the media, in microns.
    gfx::Size size_microns;
    // Platform specific id to map it back to the particular media.
    std::string vendor_id;

    bool IsDefault() const {
      return size_microns.IsEmpty() && vendor_id.empty();
    }
  };

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  using AdvancedSettings = std::map<std::string, base::Value>;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  PrintSettings();
  PrintSettings(const PrintSettings&) = delete;
  PrintSettings& operator=(const PrintSettings&) = delete;
  ~PrintSettings();

  // Reinitialize the settings to the default values.
  void Clear();

  void SetCustomMargins(const PageMargins& requested_margins_in_points);
  const PageMargins& requested_custom_margins_in_points() const {
    return requested_custom_margins_in_points_;
  }
  void set_margin_type(mojom::MarginType margin_type) {
    margin_type_ = margin_type;
  }
  mojom::MarginType margin_type() const { return margin_type_; }

  // Updates the orientation and flip the page if needed.
  void SetOrientation(bool landscape);
  bool landscape() const { return landscape_; }

  // Updates user requested media.
  void set_requested_media(const RequestedMedia& media) {
    requested_media_ = media;
  }
  // Media properties requested by the user. Translated into device media by the
  // platform specific layers.
  const RequestedMedia& requested_media() const { return requested_media_; }

  // Set printer printable area in in device units.
  // Some platforms already provide flipped area. Set |landscape_needs_flip|
  // to false on those platforms to avoid double flipping.
  // This method assumes correct DPI is already set.
  void SetPrinterPrintableArea(const gfx::Size& physical_size_device_units,
                               const gfx::Rect& printable_area_device_units,
                               bool landscape_needs_flip);
  const PageSetup& page_setup_device_units() const {
    return page_setup_device_units_;
  }

  void set_device_name(const base::string16& device_name) {
    device_name_ = device_name;
  }
  const base::string16& device_name() const { return device_name_; }

  void set_dpi(int dpi) { dpi_ = gfx::Size(dpi, dpi); }
  void set_dpi_xy(int dpi_horizontal, int dpi_vertical) {
    dpi_ = gfx::Size(dpi_horizontal, dpi_vertical);
  }

  int dpi() const { return std::max(dpi_.width(), dpi_.height()); }
  int dpi_horizontal() const { return dpi_.width(); }
  int dpi_vertical() const { return dpi_.height(); }
  const gfx::Size& dpi_size() const { return dpi_; }

  void set_scale_factor(double scale_factor) { scale_factor_ = scale_factor; }
  double scale_factor() const { return scale_factor_; }

  void set_rasterize_pdf(bool rasterize_pdf) { rasterize_pdf_ = rasterize_pdf; }
  bool rasterize_pdf() const { return rasterize_pdf_; }

  void set_supports_alpha_blend(bool supports_alpha_blend) {
    supports_alpha_blend_ = supports_alpha_blend;
  }
  bool supports_alpha_blend() const { return supports_alpha_blend_; }

  int device_units_per_inch() const {
#if defined(OS_MAC)
    return 72;
#else   // defined(OS_MAC)
    return dpi();
#endif  // defined(OS_MAC)
  }

  void set_ranges(const PageRanges& ranges) { ranges_ = ranges; }
  const PageRanges& ranges() const { return ranges_; }

  void set_selection_only(bool selection_only) {
    selection_only_ = selection_only;
  }
  bool selection_only() const { return selection_only_; }

  void set_should_print_backgrounds(bool should_print_backgrounds) {
    should_print_backgrounds_ = should_print_backgrounds;
  }
  bool should_print_backgrounds() const { return should_print_backgrounds_; }

  void set_display_header_footer(bool display_header_footer) {
    display_header_footer_ = display_header_footer;
  }
  bool display_header_footer() const { return display_header_footer_; }

  void set_title(const base::string16& title) { title_ = title; }
  const base::string16& title() const { return title_; }

  void set_url(const base::string16& url) { url_ = url; }
  const base::string16& url() const { return url_; }

  void set_collate(bool collate) { collate_ = collate; }
  bool collate() const { return collate_; }

  void set_color(mojom::ColorModel color) { color_ = color; }
  mojom::ColorModel color() const { return color_; }

  void set_copies(int copies) { copies_ = copies; }
  int copies() const { return copies_; }

  void set_duplex_mode(mojom::DuplexMode duplex_mode) {
    duplex_mode_ = duplex_mode;
  }
  mojom::DuplexMode duplex_mode() const { return duplex_mode_; }

#if defined(OS_WIN)
  void set_print_text_with_gdi(bool use_gdi) { print_text_with_gdi_ = use_gdi; }
  bool print_text_with_gdi() const { return print_text_with_gdi_; }

  void set_printer_type(PrinterType type) { printer_type_ = type; }
  bool printer_is_textonly() const {
    return printer_type_ == PrinterType::TYPE_TEXTONLY;
  }
  bool printer_is_xps() const { return printer_type_ == PrinterType::TYPE_XPS; }
  bool printer_is_ps2() const {
    return printer_type_ == PrinterType::TYPE_POSTSCRIPT_LEVEL2;
  }
  bool printer_is_ps3() const {
    return printer_type_ == PrinterType::TYPE_POSTSCRIPT_LEVEL3;
  }
#endif

  void set_is_modifiable(bool is_modifiable) { is_modifiable_ = is_modifiable; }
  bool is_modifiable() const { return is_modifiable_; }

  int pages_per_sheet() const { return pages_per_sheet_; }
  void set_pages_per_sheet(int pages_per_sheet) {
    pages_per_sheet_ = pages_per_sheet;
  }

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  AdvancedSettings& advanced_settings() { return advanced_settings_; }
  const AdvancedSettings& advanced_settings() const {
    return advanced_settings_;
  }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  void set_send_user_info(bool send_user_info) {
    send_user_info_ = send_user_info;
  }
  bool send_user_info() const { return send_user_info_; }

  void set_username(const std::string& username) { username_ = username; }
  const std::string& username() const { return username_; }

  void set_pin_value(const std::string& pin_value) { pin_value_ = pin_value; }
  const std::string& pin_value() const { return pin_value_; }
#endif  // defined(OS_CHROMEOS)

  // Cookie generator. It is used to initialize PrintedDocument with its
  // associated PrintSettings, to be sure that each generated PrintedPage is
  // correctly associated with its corresponding PrintedDocument.
  static int NewCookie();

 private:
  // Multi-page printing. Each PageRange describes a from-to page combination.
  // This permits printing selected pages only.
  PageRanges ranges_;

  // Indicates if the user only wants to print the current selection.
  bool selection_only_;

  // Indicates what kind of margins should be applied to the printable area.
  mojom::MarginType margin_type_;

  // Strings to be printed as headers and footers if requested by the user.
  base::string16 title_;
  base::string16 url_;

  // True if the user wants headers and footers to be displayed.
  bool display_header_footer_;

  // True if the user wants to print CSS backgrounds.
  bool should_print_backgrounds_;

  // True if the user wants to print with collate.
  bool collate_;

  // Color model type for the printer to use.
  mojom::ColorModel color_;

  // Number of copies user wants to print.
  int copies_;

  // Duplex type user wants to use.
  mojom::DuplexMode duplex_mode_;

  // Printer device name as opened by the OS.
  base::string16 device_name_;

  // Media requested by the user.
  RequestedMedia requested_media_;

  // Page setup in device units.
  PageSetup page_setup_device_units_;

  // Printer's device effective dots per inch in both axes. The two values will
  // generally be identical. However, on Windows, there are a few rare printers
  // that support resolutions with different DPI in different dimensions.
  gfx::Size dpi_;

  // Scale factor
  double scale_factor_;

  // True if PDF should be printed as a raster PDF
  bool rasterize_pdf_;

  // Is the orientation landscape or portrait.
  bool landscape_;

  // True if this printer supports AlphaBlend.
  bool supports_alpha_blend_;

#if defined(OS_WIN)
  // True to print text with GDI.
  bool print_text_with_gdi_;

  PrinterType printer_type_;
#endif

  bool is_modifiable_;

  // If margin type is custom, this is what was requested.
  PageMargins requested_custom_margins_in_points_;

  // Number of pages per sheet.
  int pages_per_sheet_;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Advanced settings.
  AdvancedSettings advanced_settings_;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  // Whether to send user info.
  bool send_user_info_;

  // Username if it's required by the printer.
  std::string username_;

  // PIN code entered by the user.
  std::string pin_value_;
#endif
};

}  // namespace printing

#endif  // PRINTING_PRINT_SETTINGS_H_
