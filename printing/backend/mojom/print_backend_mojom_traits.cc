// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/mojom/print_backend_mojom_traits.h"

#include <map>

#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

// Implementations of std::less<> here are for purposes of detecting duplicate
// entries in arrays.  They do not require strict checks of all fields, but
// instead focus on identifying attributes that would be used to clearly
// distinguish properties to a user.  E.g., if two entries have the same
// displayable name but different corresponding values, consider that to be a
// duplicate for these purposes.
namespace std {

template <>
struct less<::gfx::Size> {
  bool operator()(const ::gfx::Size& lhs, const ::gfx::Size& rhs) const {
    if (lhs.width() < rhs.width())
      return true;
    return lhs.height() < rhs.height();
  }
};

template <>
struct less<::printing::PrinterSemanticCapsAndDefaults::Paper> {
  bool operator()(
      const ::printing::PrinterSemanticCapsAndDefaults::Paper& lhs,
      const ::printing::PrinterSemanticCapsAndDefaults::Paper& rhs) const {
    if (lhs.display_name() < rhs.display_name()) {
      return true;
    }
    return lhs.vendor_id() < rhs.vendor_id();
  }
};

#if BUILDFLAG(IS_CHROMEOS)
template <>
struct less<::printing::AdvancedCapability> {
  bool operator()(const ::printing::AdvancedCapability& lhs,
                  const ::printing::AdvancedCapability& rhs) const {
    if (lhs.name < rhs.name)
      return true;
    return lhs.display_name < rhs.display_name;
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace std

namespace mojo {

namespace {
template <class Key>
class DuplicateChecker {
 public:
  bool HasDuplicates(const std::vector<Key>& items) {
    std::map<Key, bool> items_encountered;
    for (auto it = items.begin(); it != items.end(); ++it) {
      auto found = items_encountered.find(*it);
      if (found != items_encountered.end())
        return true;
      items_encountered[*it] = true;
    }
    return false;
  }
};

}  // namespace

// static
bool StructTraits<printing::mojom::PrinterBasicInfoDataView,
                  printing::PrinterBasicInfo>::
    Read(printing::mojom::PrinterBasicInfoDataView data,
         printing::PrinterBasicInfo* out) {
  if (!data.ReadPrinterName(&out->printer_name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadDisplayName(&out->display_name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadPrinterDescription(&out->printer_description)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadOptions(&out->options)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }

  // There should be a non-empty value for `printer_name` since it needs to
  // uniquely identify the printer with the operating system among multiple
  // possible destinations.
  if (out->printer_name.empty()) {
    DLOG(ERROR) << "The printer name must not be empty.";
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  // There should be a non-empty value for `display_name` since it needs to
  // uniquely identify the printer in user dialogs among multiple possible
  // destinations.
  if (out->display_name.empty()) {
    DLOG(ERROR) << "The printer's display name must not be empty.";
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }

  return true;
}

// static
bool StructTraits<printing::mojom::PaperDataView,
                  printing::PrinterSemanticCapsAndDefaults::Paper>::
    Read(printing::mojom::PaperDataView data,
         printing::PrinterSemanticCapsAndDefaults::Paper* out) {
  std::string display_name;
  std::string vendor_id;
  gfx::Size size_um;
  std::optional<gfx::Rect> maybe_printable_area_um;
  if (!data.ReadDisplayName(&display_name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadVendorId(&vendor_id)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadSizeUm(&size_um)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadPrintableAreaUm(&maybe_printable_area_um)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  int max_height_um = data.max_height_um();
  bool has_borderless_variant = data.has_borderless_variant();

  // For backwards compatibility, allow printable area to be missing. Set the
  // default printable area to be the page size.
  gfx::Rect printable_area_um =
      maybe_printable_area_um.value_or(gfx::Rect(size_um));

  // Allow empty Papers, since PrinterSemanticCapsAndDefaults can have empty
  // default Papers.
  if (display_name.empty() && vendor_id.empty() && size_um.IsEmpty() &&
      printable_area_um.IsEmpty() && max_height_um == 0) {
    *out = printing::PrinterSemanticCapsAndDefaults::Paper();
    return true;
  }

  // If `max_height_um` is specified, ensure it's larger than size.
  if (max_height_um > 0 && max_height_um < size_um.height()) {
    base::debug::Alias(&data);
    base::debug::Alias(&max_height_um);
    base::debug::Alias(&size_um);
    base::debug::DumpWithoutCrashing();
    return false;
  }

  // Invalid if the printable area is empty or if the printable area is out of
  // bounds of the paper size.  `max_height_um` doesn't need to be checked here
  // since `printable_area_um` is always relative to `size_um`.
  if (printable_area_um.IsEmpty() ||
      !gfx::Rect(size_um).Contains(printable_area_um)) {
    base::debug::Alias(&data);
    base::debug::Alias(&printable_area_um);
    base::debug::Alias(&size_um);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  *out = printing::PrinterSemanticCapsAndDefaults::Paper(
      display_name, vendor_id, size_um, printable_area_um, max_height_um,
      has_borderless_variant);
  return true;
}

// static
bool StructTraits<printing::mojom::MediaTypeDataView,
                  printing::PrinterSemanticCapsAndDefaults::MediaType>::
    Read(printing::mojom::MediaTypeDataView data,
         printing::PrinterSemanticCapsAndDefaults::MediaType* out) {
  if (!data.ReadDisplayName(&out->display_name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadVendorId(&out->vendor_id)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
// static
printing::mojom::AdvancedCapabilityType
EnumTraits<printing::mojom::AdvancedCapabilityType,
           ::printing::AdvancedCapability::Type>::
    ToMojom(::printing::AdvancedCapability::Type input) {
  switch (input) {
    case ::printing::AdvancedCapability::Type::kBoolean:
      return printing::mojom::AdvancedCapabilityType::kBoolean;
    case ::printing::AdvancedCapability::Type::kFloat:
      return printing::mojom::AdvancedCapabilityType::kFloat;
    case ::printing::AdvancedCapability::Type::kInteger:
      return printing::mojom::AdvancedCapabilityType::kInteger;
    case ::printing::AdvancedCapability::Type::kString:
      return printing::mojom::AdvancedCapabilityType::kString;
  }
  NOTREACHED();
}

// static
bool EnumTraits<printing::mojom::AdvancedCapabilityType,
                ::printing::AdvancedCapability::Type>::
    FromMojom(printing::mojom::AdvancedCapabilityType input,
              ::printing::AdvancedCapability::Type* output) {
  switch (input) {
    case printing::mojom::AdvancedCapabilityType::kBoolean:
      *output = ::printing::AdvancedCapability::Type::kBoolean;
      return true;
    case printing::mojom::AdvancedCapabilityType::kFloat:
      *output = ::printing::AdvancedCapability::Type::kFloat;
      return true;
    case printing::mojom::AdvancedCapabilityType::kInteger:
      *output = ::printing::AdvancedCapability::Type::kInteger;
      return true;
    case printing::mojom::AdvancedCapabilityType::kString:
      *output = ::printing::AdvancedCapability::Type::kString;
      return true;
  }
  NOTREACHED();
}

// static
bool StructTraits<printing::mojom::AdvancedCapabilityValueDataView,
                  ::printing::AdvancedCapabilityValue>::
    Read(printing::mojom::AdvancedCapabilityValueDataView data,
         ::printing::AdvancedCapabilityValue* out) {
  if (!data.ReadName(&out->name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadDisplayName(&out->display_name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  return true;
}

// static
bool StructTraits<printing::mojom::AdvancedCapabilityDataView,
                  ::printing::AdvancedCapability>::
    Read(printing::mojom::AdvancedCapabilityDataView data,
         ::printing::AdvancedCapability* out) {
  if (!data.ReadName(&out->name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadDisplayName(&out->display_name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadType(&out->type)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadDefaultValue(&out->default_value)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadValues(&out->values)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  return true;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
// static
bool StructTraits<printing::mojom::PageOutputQualityAttributeDataView,
                  printing::PageOutputQualityAttribute>::
    Read(printing::mojom::PageOutputQualityAttributeDataView data,
         printing::PageOutputQualityAttribute* out) {
  if (!data.ReadDisplayName(&out->display_name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadName(&out->name)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  return true;
}

// static
bool StructTraits<printing::mojom::PageOutputQualityDataView,
                  printing::PageOutputQuality>::
    Read(printing::mojom::PageOutputQualityDataView data,
         printing::PageOutputQuality* out) {
  if (!data.ReadQualities(&out->qualities)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadDefaultQuality(&out->default_quality)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  return true;
}
#endif

// static
bool StructTraits<printing::mojom::PrinterSemanticCapsAndDefaultsDataView,
                  printing::PrinterSemanticCapsAndDefaults>::
    Read(printing::mojom::PrinterSemanticCapsAndDefaultsDataView data,
         printing::PrinterSemanticCapsAndDefaults* out) {
  std::optional<printing::PrinterSemanticCapsAndDefaults::MediaTypes>
      media_types;
  std::optional<printing::PrinterSemanticCapsAndDefaults::MediaType>
      default_media_type;

  out->collate_capable = data.collate_capable();
  out->collate_default = data.collate_default();
  out->copies_max = data.copies_max();
  if (!data.ReadDuplexModes(&out->duplex_modes)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadDuplexDefault(&out->duplex_default)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  out->color_changeable = data.color_changeable();
  out->color_default = data.color_default();
  if (!data.ReadColorModel(&out->color_model)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadBwModel(&out->bw_model)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadPapers(&out->papers)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadUserDefinedPapers(&out->user_defined_papers)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadDefaultPaper(&out->default_paper)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadDpis(&out->dpis)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadDefaultDpi(&out->default_dpi)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  out->pin_supported = data.pin_supported();
  if (!data.ReadAdvancedCapabilities(&out->advanced_capabilities)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Extra validity checks.

  // Can not have less than one copy.
  if (out->copies_max < 1) {
    DLOG(ERROR) << "Must have copies_max greater than zero.";
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }

  // There should not be duplicates in certain arrays.
  DuplicateChecker<printing::mojom::DuplexMode> duplex_modes_dup_checker;
  if (duplex_modes_dup_checker.HasDuplicates(out->duplex_modes)) {
    DLOG(ERROR) << "Duplicate duplex_modes detected.";
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }

  DuplicateChecker<printing::PrinterSemanticCapsAndDefaults::Paper>
      user_defined_papers_dup_checker;
  if (user_defined_papers_dup_checker.HasDuplicates(out->user_defined_papers)) {
    DLOG(ERROR) << "Duplicate user_defined_papers detected.";
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  DuplicateChecker<printing::AdvancedCapability>
      advanced_capabilities_dup_checker;
  if (advanced_capabilities_dup_checker.HasDuplicates(
          out->advanced_capabilities)) {
    DLOG(ERROR) << "Duplicate advanced_capabilities detected.";
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  if (!data.ReadPageOutputQuality(&out->page_output_quality)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }
  DuplicateChecker<printing::PageOutputQualityAttribute>
      page_output_quality_dup_checker;
  if (out->page_output_quality) {
    printing::PageOutputQualityAttributes qualities =
        out->page_output_quality->qualities;
    std::optional<std::string> default_quality =
        out->page_output_quality->default_quality;

    // If non-null `default_quality`, there should be a matching element in
    // `qualities` array.
    if (default_quality) {
      if (!base::Contains(qualities, *default_quality,
                          &printing::PageOutputQualityAttribute::name)) {
        DLOG(ERROR) << "Non-null default quality, but page output qualities "
                       "does not contain default quality";
        base::debug::Alias(&data);
        base::debug::Alias(&default_quality);
        base::debug::Alias(&qualities);
        base::debug::DumpWithoutCrashing();
        return false;
      }
    }

    // There should be no duplicates in `qualities` array.
    if (page_output_quality_dup_checker.HasDuplicates(qualities)) {
      DLOG(ERROR) << "Duplicate page output qualities detected.";
      base::debug::Alias(&data);
      base::debug::Alias(&qualities);
      base::debug::DumpWithoutCrashing();
      return false;
    }
  }
#endif

  if (!data.ReadMediaTypes(&media_types) ||
      !data.ReadDefaultMediaType(&default_media_type)) {
    base::debug::Alias(&data);
    base::debug::DumpWithoutCrashing();
    return false;
  }

  if (media_types.has_value()) {
    out->media_types = media_types.value();
  }
  if (default_media_type.has_value()) {
    out->default_media_type = default_media_type.value();
  }

  return true;
}

}  // namespace mojo
