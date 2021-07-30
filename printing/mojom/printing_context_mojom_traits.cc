// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/mojom/printing_context_mojom_traits.h"

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

}  // namespace mojo
