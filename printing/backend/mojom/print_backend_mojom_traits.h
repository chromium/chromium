// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_MOJOM_PRINT_BACKEND_MOJOM_TRAITS_H_
#define PRINTING_BACKEND_MOJOM_PRINT_BACKEND_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "printing/backend/mojom/print_backend.mojom-shared.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct StructTraits<printing::mojom::PrinterBasicInfoDataView,
                    printing::PrinterBasicInfo> {
  static const std::string& printer_name(const printing::PrinterBasicInfo& i) {
    return i.printer_name;
  }
  static const std::string& display_name(const printing::PrinterBasicInfo& i) {
    return i.display_name;
  }
  static const std::string& printer_description(
      const printing::PrinterBasicInfo& i) {
    return i.printer_description;
  }
  static int printer_status(const printing::PrinterBasicInfo& i) {
    return i.printer_status;
  }
  static bool is_default(const printing::PrinterBasicInfo& i) {
    return i.is_default;
  }
  static const printing::PrinterBasicInfoOptions& options(
      const printing::PrinterBasicInfo& i) {
    return i.options;
  }

  static bool Read(printing::mojom::PrinterBasicInfoDataView data,
                   printing::PrinterBasicInfo* out);
};

template <>
struct StructTraits<printing::mojom::PaperDataView,
                    printing::PrinterSemanticCapsAndDefaults::Paper> {
  static const std::string& display_name(
      const printing::PrinterSemanticCapsAndDefaults::Paper& p) {
    return p.display_name();
  }
  static const std::string& vendor_id(
      const printing::PrinterSemanticCapsAndDefaults::Paper& p) {
    return p.vendor_id();
  }
  static const gfx::Size& size_um(
      const printing::PrinterSemanticCapsAndDefaults::Paper& p) {
    return p.size_um();
  }
  static const gfx::Rect& printable_area_um(
      const printing::PrinterSemanticCapsAndDefaults::Paper& p) {
    return p.printable_area_um();
  }
  static int max_height_um(
      const printing::PrinterSemanticCapsAndDefaults::Paper& p) {
    return p.max_height_um();
  }
  static bool has_borderless_variant(
      const printing::PrinterSemanticCapsAndDefaults::Paper& p) {
    return p.has_borderless_variant();
  }

  static bool Read(printing::mojom::PaperDataView data,
                   printing::PrinterSemanticCapsAndDefaults::Paper* out);
};

template <>
struct StructTraits<printing::mojom::MediaTypeDataView,
                    printing::PrinterSemanticCapsAndDefaults::MediaType> {
  static const std::string& display_name(
      const printing::PrinterSemanticCapsAndDefaults::MediaType& p) {
    return p.display_name;
  }
  static const std::string& vendor_id(
      const printing::PrinterSemanticCapsAndDefaults::MediaType& p) {
    return p.vendor_id;
  }

  static bool Read(printing::mojom::MediaTypeDataView data,
                   printing::PrinterSemanticCapsAndDefaults::MediaType* out);
};

#if BUILDFLAG(IS_CHROMEOS)
template <>
struct EnumTraits<printing::mojom::AdvancedCapabilityType,
                  ::printing::AdvancedCapability::Type> {
  static printing::mojom::AdvancedCapabilityType ToMojom(
      ::printing::AdvancedCapability::Type input);
  static bool FromMojom(printing::mojom::AdvancedCapabilityType input,
                        ::printing::AdvancedCapability::Type* output);
};

template <>
struct StructTraits<printing::mojom::AdvancedCapabilityValueDataView,
                    ::printing::AdvancedCapabilityValue> {
  static const std::string& name(const ::printing::AdvancedCapabilityValue& v) {
    return v.name;
  }
  static const std::string& display_name(
      const ::printing::AdvancedCapabilityValue& v) {
    return v.display_name;
  }

  static bool Read(printing::mojom::AdvancedCapabilityValueDataView data,
                   ::printing::AdvancedCapabilityValue* out);
};

template <>
struct StructTraits<printing::mojom::AdvancedCapabilityDataView,
                    ::printing::AdvancedCapability> {
  static const std::string& name(const ::printing::AdvancedCapability& c) {
    return c.name;
  }
  static const std::string& display_name(
      const ::printing::AdvancedCapability& c) {
    return c.display_name;
  }
  static ::printing::AdvancedCapability::Type type(
      const ::printing::AdvancedCapability& c) {
    return c.type;
  }
  static const std::string& default_value(
      const ::printing::AdvancedCapability& c) {
    return c.default_value;
  }
  static const std::vector<::printing::AdvancedCapabilityValue>& values(
      const ::printing::AdvancedCapability& c) {
    return c.values;
  }

  static bool Read(printing::mojom::AdvancedCapabilityDataView data,
                   ::printing::AdvancedCapability* out);
};
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
template <>
struct StructTraits<printing::mojom::PageOutputQualityAttributeDataView,
                    ::printing::PageOutputQualityAttribute> {
  static const std::string& display_name(
      const ::printing::PageOutputQualityAttribute& p) {
    return p.display_name;
  }

  static const std::string& name(
      const ::printing::PageOutputQualityAttribute& p) {
    return p.name;
  }

  static bool Read(printing::mojom::PageOutputQualityAttributeDataView data,
                   printing::PageOutputQualityAttribute* out);
};

template <>
struct StructTraits<printing::mojom::PageOutputQualityDataView,
                    printing::PageOutputQuality> {
  static const std::vector<::printing::PageOutputQualityAttribute>& qualities(
      const ::printing::PageOutputQuality& p) {
    return p.qualities;
  }

  static const std::optional<std::string>& default_quality(
      const ::printing::PageOutputQuality& p) {
    return p.default_quality;
  }

  static bool Read(printing::mojom::PageOutputQualityDataView data,
                   printing::PageOutputQuality* out);
};
#endif  // BUILDFLAG(IS_WIN)

template <>
struct StructTraits<printing::mojom::PrinterSemanticCapsAndDefaultsDataView,
                    printing::PrinterSemanticCapsAndDefaults> {
  static bool collate_capable(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.collate_capable;
  }
  static bool collate_default(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.collate_default;
  }
  static int32_t copies_max(const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.copies_max;
  }
  static const std::vector<printing::mojom::DuplexMode>& duplex_modes(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.duplex_modes;
  }
  static printing::mojom::DuplexMode duplex_default(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.duplex_default;
  }
  static bool color_changeable(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.color_changeable;
  }
  static bool color_default(const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.color_default;
  }
  static printing::mojom::ColorModel color_model(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.color_model;
  }
  static printing::mojom::ColorModel bw_model(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.bw_model;
  }
  static const std::vector<printing::PrinterSemanticCapsAndDefaults::Paper>&
  papers(const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.papers;
  }
  static const std::vector<printing::PrinterSemanticCapsAndDefaults::Paper>&
  user_defined_papers(const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.user_defined_papers;
  }
  static const printing::PrinterSemanticCapsAndDefaults::Paper& default_paper(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.default_paper;
  }
  static const std::vector<printing::PrinterSemanticCapsAndDefaults::MediaType>&
  media_types(const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.media_types;
  }
  static const printing::PrinterSemanticCapsAndDefaults::MediaType&
  default_media_type(const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.default_media_type;
  }
  static const std::vector<gfx::Size>& dpis(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.dpis;
  }
  static const gfx::Size& default_dpi(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.default_dpi;
  }

#if BUILDFLAG(IS_CHROMEOS)
  static bool pin_supported(const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.pin_supported;
  }
  static const printing::AdvancedCapabilities& advanced_capabilities(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.advanced_capabilities;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  static const std::optional<printing::PageOutputQuality>& page_output_quality(
      const printing::PrinterSemanticCapsAndDefaults& p) {
    return p.page_output_quality;
  }
#endif  // BUILDFLAG(IS_WIN)

  static bool Read(printing::mojom::PrinterSemanticCapsAndDefaultsDataView data,
                   printing::PrinterSemanticCapsAndDefaults* out);
};

}  // namespace mojo

#endif  // PRINTING_BACKEND_MOJOM_PRINT_BACKEND_MOJOM_TRAITS_H_
