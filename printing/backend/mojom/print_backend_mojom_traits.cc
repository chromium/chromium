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

}  // namespace mojo
