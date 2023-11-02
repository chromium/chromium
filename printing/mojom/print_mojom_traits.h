// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_MOJOM_PRINT_MOJOM_TRAITS_H_
#define PRINTING_MOJOM_PRINT_MOJOM_TRAITS_H_

#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"

namespace mojo {

template <>
struct StructTraits<printing::mojom::PageRangeDataView, printing::PageRange> {
  static uint32_t from(const printing::PageRange& r) { return r.from; }
  static uint32_t to(const printing::PageRange& r) { return r.to; }

  static bool Read(printing::mojom::PageRangeDataView data,
                   printing::PageRange* out);
};

}  // namespace mojo

#endif  // PRINTING_MOJOM_PRINT_MOJOM_TRAITS_H_
