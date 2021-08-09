// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_MOJOM_PRINTING_CONTEXT_MOJOM_TRAITS_H_
#define PRINTING_MOJOM_PRINTING_CONTEXT_MOJOM_TRAITS_H_

#include "printing/mojom/printing_context.mojom-shared.h"
#include "printing/page_range.h"
#include "printing/page_setup.h"
#include "printing/print_settings.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct StructTraits<printing::mojom::PageMarginsDataView,
                    printing::PageMargins> {
  static int32_t header(const printing::PageMargins& m) { return m.header; }
  static int32_t footer(const printing::PageMargins& m) { return m.footer; }
  static int32_t left(const printing::PageMargins& m) { return m.left; }
  static int32_t right(const printing::PageMargins& m) { return m.right; }
  static int32_t top(const printing::PageMargins& m) { return m.top; }
  static int32_t bottom(const printing::PageMargins& m) { return m.bottom; }

  static bool Read(printing::mojom::PageMarginsDataView data,
                   printing::PageMargins* out);
};

template <>
struct StructTraits<printing::mojom::PageSetupDataView, printing::PageSetup> {
  static const gfx::Size& physical_size(const printing::PageSetup& s) {
    return s.physical_size();
  }
  static const gfx::Rect& printable_area(const printing::PageSetup& s) {
    return s.printable_area();
  }
  static const gfx::Rect& overlay_area(const printing::PageSetup& s) {
    return s.overlay_area();
  }
  static const gfx::Rect& content_area(const printing::PageSetup& s) {
    return s.content_area();
  }
  static const printing::PageMargins& effective_margins(
      const printing::PageSetup& s) {
    return s.effective_margins();
  }
  static const printing::PageMargins& requested_margins(
      const printing::PageSetup& s) {
    return s.requested_margins();
  }
  static bool forced_margins(const printing::PageSetup& s) {
    return s.forced_margins();
  }
  static int32_t text_height(const printing::PageSetup& s) {
    return s.text_height();
  }

  static bool Read(printing::mojom::PageSetupDataView data,
                   printing::PageSetup* out);
};

template <>
struct StructTraits<printing::mojom::PageRangeDataView, printing::PageRange> {
  static uint32_t from(const printing::PageRange& r) { return r.from; }
  static uint32_t to(const printing::PageRange& r) { return r.to; }

  static bool Read(printing::mojom::PageRangeDataView data,
                   printing::PageRange* out);
};

template <>
struct StructTraits<printing::mojom::RequestedMediaDataView,
                    printing::PrintSettings::RequestedMedia> {
  static const gfx::Size& size_microns(
      const printing::PrintSettings::RequestedMedia& r) {
    return r.size_microns;
  }
  static const std::string& vendor_id(
      const printing::PrintSettings::RequestedMedia& r) {
    return r.vendor_id;
  }

  static bool Read(printing::mojom::RequestedMediaDataView data,
                   printing::PrintSettings::RequestedMedia* out);
};

}  // namespace mojo

#endif  // PRINTING_MOJOM_PRINTING_CONTEXT_MOJOM_TRAITS_H_
