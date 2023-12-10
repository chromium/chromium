// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/mojom/printing_context_mojom_traits.h"

#include <string>

#include "build/build_config.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_setup.h"
#include "printing/print_settings.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
#include "base/numerics/safe_conversions.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "mojo/public/mojom/base/values.mojom.h"
#endif

namespace mojo {

// static
bool StructTraits<printing::mojom::PageMarginsDataView, printing::PageMargins>::
    Read(printing::mojom::PageMarginsDataView data,
         printing::PageMargins* out) {
  out->header = data.header();
  out->footer = data.footer();
  out->left = data.left();
  out->right = data.right();
  out->top = data.top();
  out->bottom = data.bottom();
  return true;
}

// static
bool StructTraits<printing::mojom::PageSetupDataView, printing::PageSetup>::
    Read(printing::mojom::PageSetupDataView data, printing::PageSetup* out) {
  gfx::Size physical_size;
  gfx::Rect printable_area;
  gfx::Rect overlay_area;
  gfx::Rect content_area;
  printing::PageMargins effective_margins;
  printing::PageMargins requested_margins;

  if (!data.ReadPhysicalSize(&physical_size) ||
      !data.ReadPrintableArea(&printable_area) ||
      !data.ReadOverlayArea(&overlay_area) ||
      !data.ReadContentArea(&content_area) ||
      !data.ReadEffectiveMargins(&effective_margins) ||
      !data.ReadRequestedMargins(&requested_margins)) {
    return false;
  }

  printing::PageSetup page_setup(physical_size, printable_area,
                                 requested_margins, data.forced_margins(),
                                 data.text_height());

  if (page_setup.overlay_area() != overlay_area)
    return false;
  if (page_setup.content_area() != content_area)
    return false;
  if (page_setup.effective_margins() != effective_margins) {
    return false;
  }

  *out = page_setup;
  return true;
}

// static
bool StructTraits<printing::mojom::RequestedMediaDataView,
                  printing::PrintSettings::RequestedMedia>::
    Read(printing::mojom::RequestedMediaDataView data,
         printing::PrintSettings::RequestedMedia* out) {
  return data.ReadSizeMicrons(&out->size_microns) &&
         data.ReadVendorId(&out->vendor_id);
}

// static
bool StructTraits<
    printing::mojom::PrintSettingsDataView,
    printing::PrintSettings>::Read(printing::mojom::PrintSettingsDataView data,
                                   printing::PrintSettings* out) {
  printing::PageRanges ranges;
  if (!data.ReadRanges(&ranges))
    return false;
  out->set_ranges(ranges);

  out->set_selection_only(data.selection_only());
  out->set_margin_type(data.margin_type());

  std::u16string title;
  if (!data.ReadTitle(&title))
    return false;
  out->set_title(title);

  std::u16string url;
  if (!data.ReadUrl(&url))
    return false;
  out->set_url(url);

  out->set_display_header_footer(data.display_header_footer());
  out->set_should_print_backgrounds(data.should_print_backgrounds());
  out->set_collate(data.collate());
  out->set_color(data.color());
  out->set_copies(data.copies());
  out->set_duplex_mode(data.duplex_mode());

  std::u16string device_name;
  if (!data.ReadDeviceName(&device_name))
    return false;
  out->set_device_name(device_name);

  printing::PrintSettings::RequestedMedia requested_media;
  if (!data.ReadRequestedMedia(&requested_media))
    return false;
  out->set_requested_media(requested_media);

  // Must set orientation before page setup, otherwise it can introduce an extra
  // flipping of landscape page size dimensions.
  out->SetOrientation(data.landscape());
  printing::PageSetup page_setup;
  if (!data.ReadPageSetupDeviceUnits(&page_setup))
    return false;
  out->set_page_setup_device_units(page_setup);

  std::string media_type;
  if (!data.ReadMediaType(&media_type)) {
    return false;
  }
  out->set_media_type(media_type);

  out->set_borderless(data.borderless());

  gfx::Size dpi;
  if (!data.ReadDpi(&dpi))
    return false;
  out->set_dpi_xy(dpi.width(), dpi.height());

  out->set_scale_factor(data.scale_factor());
  out->set_rasterize_pdf(data.rasterize_pdf());

#if BUILDFLAG(IS_WIN)
  out->set_printer_language_type(data.printer_language_type());
#endif  // BUILDFLAG(IS_WIN)
  out->set_is_modifiable(data.is_modifiable());

  // `SetCustomMargins()` has side effect of explicitly setting `margin_type_`
  // so only want to apply this if the type was for `kCustomMargins`.
  if (data.margin_type() == printing::mojom::MarginType::kCustomMargins) {
    printing::PageMargins requested_margins;
    if (!data.ReadRequestedCustomMarginsInPoints(&requested_margins))
      return false;
    out->SetCustomMargins(requested_margins);
  }

  out->set_pages_per_sheet(data.pages_per_sheet());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  DCHECK(out->advanced_settings().empty());
  if (!data.ReadAdvancedSettings(&out->advanced_settings()))
    return false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS)
  out->set_send_user_info(data.send_user_info());

  std::string username;
  if (!data.ReadUsername(&username))
    return false;
  out->set_username(username);

  std::string pin_value;
  if (!data.ReadPinValue(&pin_value))
    return false;
  out->set_pin_value(pin_value);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  base::Value::Dict system_print_dialog_data;
  if (!data.ReadSystemPrintDialogData(&system_print_dialog_data)) {
    return false;
  }
  // If doing system print then a dialog invoked in-browser needs to provide
  // the settings that were specified.  Such data is platform-specific.
  if (!system_print_dialog_data.empty()) {
#if BUILDFLAG(IS_MAC)
    // The dictionary must contain the destination type, page format, and
    // print settings.
    std::optional<int> destination_type = system_print_dialog_data.FindInt(
        printing::kMacSystemPrintDialogDataDestinationType);
    if (!destination_type.has_value()) {
      return false;
    }
    const base::Value::BlobStorage* page_format =
        system_print_dialog_data.FindBlob(
            printing::kMacSystemPrintDialogDataPageFormat);
    if (!page_format || page_format->empty()) {
      return false;
    }
    const base::Value::BlobStorage* print_settings =
        system_print_dialog_data.FindBlob(
            printing::kMacSystemPrintDialogDataPrintSettings);
    if (!print_settings || print_settings->empty()) {
      return false;
    }
    size_t dictionary_entries = 3;

    // Destination type must be uint16.
    if (!base::IsValueInRangeForNumericType<uint16_t>(*destination_type)) {
      return false;
    }

    // Destination format and location are optional.  If present, they must be
    // strings.
    const base::Value* destination_format = system_print_dialog_data.Find(
        printing::kMacSystemPrintDialogDataDestinationFormat);
    if (destination_format) {
      if (!destination_format->is_string()) {
        return false;
      }
      ++dictionary_entries;
    }
    const base::Value* destination_location = system_print_dialog_data.Find(
        printing::kMacSystemPrintDialogDataDestinationLocation);
    if (destination_location) {
      if (!destination_location->is_string()) {
        return false;
      }
      ++dictionary_entries;
    }

    // There should not be any other keys present.
    if (system_print_dialog_data.size() != dictionary_entries) {
      return false;
    }
#elif BUILDFLAG(IS_LINUX)
    // The dictionary must contain three strings.
    const base::Value* value = system_print_dialog_data.Find(
        printing::kLinuxSystemPrintDialogDataPrinter);
    if (!value || !value->is_string()) {
      return false;
    }
    value = system_print_dialog_data.Find(
        printing::kLinuxSystemPrintDialogDataPrintSettings);
    if (!value || !value->is_string()) {
      return false;
    }
    value = system_print_dialog_data.Find(
        printing::kLinuxSystemPrintDialogDataPageSetup);
    if (!value || !value->is_string()) {
      return false;
    }

    // There should not be any other keys present.
    if (system_print_dialog_data.size() != 3) {
      return false;
    }
#else
#error "System print dialog support not implemented for this platform."
#endif
  }

  out->set_system_print_dialog_data(std::move(system_print_dialog_data));
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)

  return true;
}

}  // namespace mojo
