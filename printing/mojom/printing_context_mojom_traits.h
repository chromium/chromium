// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_MOJOM_PRINTING_CONTEXT_MOJOM_TRAITS_H_
#define PRINTING_MOJOM_PRINTING_CONTEXT_MOJOM_TRAITS_H_

#include "printing/mojom/printing_context.mojom-shared.h"
#include "printing/page_setup.h"

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

}  // namespace mojo

#endif  // PRINTING_MOJOM_PRINTING_CONTEXT_MOJOM_TRAITS_H_
