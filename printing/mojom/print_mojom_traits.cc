// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/mojom/print_mojom_traits.h"

namespace mojo {

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

}  // namespace mojo
