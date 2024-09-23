// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_SETTINGS_H_
#define PRINTING_PRINT_SETTINGS_H_

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"
#include "printing/page_setup.h"
#include "printing/print_job_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
#include "base/values.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <map>

#include "base/values.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace printing {

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)

#if BUILDFLAG(IS_MAC)
inline constexpr char kMacSystemPrintDialogDataDestinationType[] =
    "destination_type";
inline constexpr char kMacSystemPrintDialogDataDestinationFormat[] =
    "destination_format";
inline constexpr char kMacSystemPrintDialogDataDestinationLocation[] =
    "destination_location";
inline constexpr char kMacSystemPrintDialogDataPageFormat[] = "page_format";
inline constexpr char kMacSystemPrintDialogDataPrintSettings[] =
    "print_settings";
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
inline constexpr char kLinuxSystemPrintDialogDataPrinter[] = "printer_name";
inline constexpr char kLinuxSystemPrintDialogDataPrintSettings[] =
    "print_settings";
inline constexpr char kLinuxSystemPrintDialogDataPageSetup[] = "page_setup";
#endif  // BUILDFLAG(IS_LINUX)

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)

// Convert from `color_mode` into a `color_model`.  An invalid `color_mode`
// will give a result of `mojom::ColorModel::kUnknownColorModel`.
COMPONENT_EXPORT(PRINTING_SETTINGS)
mojom::ColorModel ColorModeToColorModel(int color_mode);

// Returns true if `color_model` is color and false if it is B&W.  Callers
// are not supposed to pass in `mojom::ColorModel::kUnknownColorModel`, but
// if they do then the result will be std::nullopt.
COMPONENT_EXPORT(PRINTING_SETTINGS)
std::optional<bool> IsColorModelSelected(mojom::ColorModel color_model);

#if BUILDFLAG(USE_CUPS)
// Get the color model setting name and value for the `color_model`.
COMPONENT_EXPORT(PRINTING_SETTINGS)
void GetColorModelForModel(mojom::ColorModel color_model,
                           std::string* color_setting_name,
                           std::string* color_value);
#endif  // BUILDFLAG(USE_CUPS)

#if BUILDFLAG(USE_CUPS_IPP)
// Convert from `color_model` to a print-color-mode value from PWG 5100.13.
COMPONENT_EXPORT(PRINTING_SETTINGS)
std::string GetIppColorModelForModel(mojom::ColorModel color_model);
#endif  // BUILDFLAG(USE_CUPS_IPP)

class COMPONENT_EXPORT(PRINTING_SETTINGS) PrintSettings {
 public:
  // Media properties requested by the user. Default instance represents
  // default media selection.
  struct RequestedMedia {
    bool operator==(const RequestedMedia& other) const;
    bool IsDefault() const {
      return size_microns.IsEmpty() && vendor_id.empty();
    }

    // Size of the media, in microns.
    gfx::Size size_microns;
    // Platform specific id to map it back to the particular media.
    std::string vendor_id;
  };

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  using AdvancedSettings = std::map<std::string, base::Value>;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  PrintSettings();
  PrintSettings(const PrintSettings&);
  PrintSettings& operator=(const PrintSettings&);
  ~PrintSettings();

  bool operator==(const PrintSettings& other) const;

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
  // Some platforms already provide flipped area. Set `landscape_needs_flip`
  // to false on those platforms to avoid double flipping.
  // This method assumes correct DPI is already set.
  void SetPrinterPrintableArea(const gfx::Size& physical_size_device_units,
                               const gfx::Rect& printable_area_device_units,
                               bool landscape_needs_flip);
#if BUILDFLAG(IS_WIN)
  // Update the printer printable area for the current media using the
  // provided area in microns.
  void UpdatePrinterPrintableArea(const gfx::Rect& printable_area_um);
#endif
  const PageSetup& page_setup_device_units() const {
    return page_setup_device_units_;
  }
  // `set_page_setup_device_units()` intended to be used only for mojom
  // validation.
  void set_page_setup_device_units(const PageSetup& page_setup_device_units) {
    page_setup_device_units_ = page_setup_device_units;
  }

  void set_device_name(const std::u16string& device_name) {
    device_name_ = device_name;
  }
  const std::u16string& device_name() const { return device_name_; }

  void set_borderless(bool borderless) { borderless_ = borderless; }
  bool borderless() const { return borderless_; }

  void set_media_type(const std::string& media_type) {
    media_type_ = media_type;
  }
  const std::string& media_type() const { return media_type_; }

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

  void set_rasterize_pdf_dpi(int32_t dpi) { rasterize_pdf_dpi_ = dpi; }
  int32_t rasterize_pdf_dpi() const { return rasterize_pdf_dpi_; }

  int device_units_per_inch() const {
#if BUILDFLAG(IS_MAC)
    return kMacDeviceUnitsPerInch;
#else   // BUILDFLAG(IS_MAC)
    return dpi();
#endif  // BUILDFLAG(IS_MAC)
  }

  const gfx::Size& device_units_per_inch_size() const {
#if BUILDFLAG(IS_MAC)
    static constexpr gfx::Size kSize{kMacDeviceUnitsPerInch,
                                     kMacDeviceUnitsPerInch};
    return kSize;
#else   // BUILDFLAG(IS_MAC)
    return dpi_size();
#endif  // BUILDFLAG(IS_MAC)
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

  void set_title(const std::u16string& title) { title_ = title; }
  const std::u16string& title() const { return title_; }

  void set_url(const std::u16string& url) { url_ = url; }
  const std::u16string& url() const { return url_; }

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

#if BUILDFLAG(IS_WIN)
  void set_printer_language_type(mojom::PrinterLanguageType type) {
    printer_language_type_ = type;
  }
  mojom::PrinterLanguageType printer_language_type() const {
    return printer_language_type_;
  }
  bool printer_language_is_textonly() const {
    return printer_language_type_ == mojom::PrinterLanguageType::kTextOnly;
  }
  bool printer_language_is_xps() const {
    return printer_language_type_ == mojom::PrinterLanguageType::kXps;
  }
  bool printer_language_is_ps2() const {
    return printer_language_type_ ==
           mojom::PrinterLanguageType::kPostscriptLevel2;
  }
  bool printer_language_is_ps3() const {
    return printer_language_type_ ==
           mojom::PrinterLanguageType::kPostscriptLevel3;
  }
#endif

  void set_is_modifiable(bool is_modifiable) { is_modifiable_ = is_modifiable; }
  bool is_modifiable() const { return is_modifiable_; }

  int pages_per_sheet() const { return pages_per_sheet_; }
  void set_pages_per_sheet(int pages_per_sheet) {
    pages_per_sheet_ = pages_per_sheet;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  AdvancedSettings& advanced_settings() { return advanced_settings_; }
  const AdvancedSettings& advanced_settings() const {
    return advanced_settings_;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  void set_send_user_info(bool send_user_info) {
    send_user_info_ = send_user_info;
  }
  bool send_user_info() const { return send_user_info_; }

  void set_username(const std::string& username) { username_ = username; }
  const std::string& username() const { return username_; }

  void set_oauth_token(const std::string& oauth_token) {
    oauth_token_ = oauth_token;
  }
  const std::string& oauth_token() const { return oauth_token_; }

  void set_pin_value(const std::string& pin_value) { pin_value_ = pin_value; }
  const std::string& pin_value() const { return pin_value_; }

  void set_client_infos(std::vector<mojom::IppClientInfo> client_infos) {
    client_infos_ = std::move(client_infos);
  }
  const std::vector<mojom::IppClientInfo>& client_infos() const {
    return client_infos_;
  }

  void set_printer_manually_selected(bool printer_manually_selected) {
    printer_manually_selected_ = printer_manually_selected;
  }
  bool printer_manually_selected() const { return printer_manually_selected_; }

  void set_printer_status_reason(
      crosapi::mojom::StatusReason::Reason printer_status_reason) {
    printer_status_reason_ = printer_status_reason;
  }
  std::optional<crosapi::mojom::StatusReason::Reason> printer_status_reason()
      const {
    return printer_status_reason_;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  void set_system_print_dialog_data(base::Value::Dict data) {
    system_print_dialog_data_ = std::move(data);
  }
  const base::Value::Dict& system_print_dialog_data() const {
    return system_print_dialog_data_;
  }
#endif
  // Cookie generator. It is used to initialize `PrintedDocument` with its
  // associated `PrintSettings`, to be sure that each generated `PrintedPage`
  // is correctly associated with its corresponding `PrintedDocument`.
  static int NewCookie();

  // Creates an invalid cookie for use in situations where the cookie needs to
  // be marked as invalid.
  static int NewInvalidCookie();

 private:
#if BUILDFLAG(IS_MAC)
  static constexpr int kMacDeviceUnitsPerInch = 72;
#endif

  // Multi-page printing. Each `PageRange` describes a from-to page combination.
  // This permits printing selected pages only.
  PageRanges ranges_;

  // Indicates if the user only wants to print the current selection.
  bool selection_only_;

  // Indicates what kind of margins should be applied to the printable area.
  mojom::MarginType margin_type_;

  // Strings to be printed as headers and footers if requested by the user.
  std::u16string title_;
  std::u16string url_;

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
  std::u16string device_name_;

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  // Platform-specific print settings captured from a system print dialog.
  // The settings are captured in the browser process for transmission to
  // the Print Backend service for OOP printing.
  base::Value::Dict system_print_dialog_data_;
#endif

  // Media requested by the user.
  RequestedMedia requested_media_;

  // Page setup in device units.
  PageSetup page_setup_device_units_;

  // Whether the user has requested borderless (zero margin) printing.
  bool borderless_;

  // Media type requested by the user.
  std::string media_type_;

  // Printer's device effective dots per inch in both axes. The two values will
  // generally be identical. However, on Windows, there are a few rare printers
  // that support resolutions with different DPI in different dimensions.
  gfx::Size dpi_;

  // Scale factor
  double scale_factor_;

  // True if PDF should be printed as a raster PDF
  bool rasterize_pdf_;

  // The DPI which overrides the calculated value normally used when
  // rasterizing a PDF.  A non-positive value would be an invalid choice of a
  // DPI and indicates no override.
  int32_t rasterize_pdf_dpi_;

  // Is the orientation landscape or portrait.
  bool landscape_;

#if BUILDFLAG(IS_WIN)
  mojom::PrinterLanguageType printer_language_type_;
#endif

  bool is_modifiable_;

  // If margin type is custom, this is what was requested.
  PageMargins requested_custom_margins_in_points_;

  // Number of pages per sheet.
  int pages_per_sheet_;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Advanced settings.
  AdvancedSettings advanced_settings_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  // Whether to send user info.
  bool send_user_info_;

  // Username if it's required by the printer.
  std::string username_;

  // OAuth access token if it's required by the printer.
  std::string oauth_token_;

  // PIN code entered by the user.
  std::string pin_value_;

  // Value of the 'client-info' that will be sent to the printer.
  // Should only be set for printers that support 'client-info'.
  std::vector<mojom::IppClientInfo> client_infos_;

  // True if the user selects to print to a different printer than the original
  // destination shown when Print Preview opens.
  bool printer_manually_selected_;

  // The printer status reason shown for the selected printer at the time print
  // is requested. Only local CrOS printers set printer statuses.
  std::optional<crosapi::mojom::StatusReason::Reason> printer_status_reason_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace printing

#endif  // PRINTING_PRINT_SETTINGS_H_
