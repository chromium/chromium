// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/mojom/printing_context_mojom_traits.h"

#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

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
  if (!effective_margins.Equals(page_setup.effective_margins()))
    return false;

  *out = page_setup;
  return true;
}

// static
bool StructTraits<printing::mojom::PageRangeDataView, printing::PageRange>::
    Read(printing::mojom::PageRangeDataView data, printing::PageRange* out) {
  out->from = data.from();
  out->to = data.to();

  // A range should represent increasing page numbers, not to be used to
  // indicate processing pages backwards.
  if (out->from > out->to)
    return false;

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

}  // namespace mojo
