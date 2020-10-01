// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_MOJOM_PRINT_BACKEND_MOJOM_TRAITS_H_
#define PRINTING_BACKEND_MOJOM_PRINT_BACKEND_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "printing/backend/mojom/print_backend.mojom-shared.h"
#include "printing/backend/print_backend.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct StructTraits<printing::mojom::PaperDataView,
                    printing::PrinterSemanticCapsAndDefaults::Paper> {
  static const std::string& display_name(
      const printing::PrinterSemanticCapsAndDefaults::Paper& p) {
    return p.display_name;
  }
  static const std::string& vendor_id(
      const printing::PrinterSemanticCapsAndDefaults::Paper& p) {
    return p.vendor_id;
  }
  static const gfx::Size& size_um(
      const printing::PrinterSemanticCapsAndDefaults::Paper& p) {
    return p.size_um;
  }

  static bool Read(printing::mojom::PaperDataView data,
                   printing::PrinterSemanticCapsAndDefaults::Paper* out);
};

#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)

}  // namespace mojo

#endif  // PRINTING_BACKEND_MOJOM_PRINT_BACKEND_MOJOM_TRAITS_H_
