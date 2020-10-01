// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/mojom/print_backend_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<printing::mojom::PaperDataView,
                  printing::PrinterSemanticCapsAndDefaults::Paper>::
    Read(printing::mojom::PaperDataView data,
         printing::PrinterSemanticCapsAndDefaults::Paper* out) {
  return data.ReadDisplayName(&out->display_name) &&
         data.ReadVendorId(&out->vendor_id) && data.ReadSizeUm(&out->size_um);
}

#if defined(OS_CHROMEOS)
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
  return printing::mojom::AdvancedCapabilityType::kString;
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
  return false;
}

// static
bool StructTraits<printing::mojom::AdvancedCapabilityValueDataView,
                  ::printing::AdvancedCapabilityValue>::
    Read(printing::mojom::AdvancedCapabilityValueDataView data,
         ::printing::AdvancedCapabilityValue* out) {
  return data.ReadName(&out->name) && data.ReadDisplayName(&out->display_name);
}

// static
bool StructTraits<printing::mojom::AdvancedCapabilityDataView,
                  ::printing::AdvancedCapability>::
    Read(printing::mojom::AdvancedCapabilityDataView data,
         ::printing::AdvancedCapability* out) {
  return data.ReadName(&out->name) &&
         data.ReadDisplayName(&out->display_name) &&
         data.ReadType(&out->type) &&
         data.ReadDefaultValue(&out->default_value) &&
         data.ReadValues(&out->values);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace mojo
