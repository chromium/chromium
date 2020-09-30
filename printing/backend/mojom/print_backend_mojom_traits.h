// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_MOJOM_PRINT_BACKEND_MOJOM_TRAITS_H_
#define PRINTING_BACKEND_MOJOM_PRINT_BACKEND_MOJOM_TRAITS_H_

#include <string>

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

}  // namespace mojo

#endif  // PRINTING_BACKEND_MOJOM_PRINT_BACKEND_MOJOM_TRAITS_H_
