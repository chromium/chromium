// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/nup_parameters.h"

#include <algorithm>
#include <cmath>

#include "base/check.h"
#include "base/notreached.h"

namespace printing {

NupParameters::NupParameters() {
  Clear();
}

void NupParameters::Clear() {
  landscape_ = false;
  num_pages_on_x_axis_ = 1;
  num_pages_on_y_axis_ = 1;
}

// static
bool NupParameters::IsSupported(int pages_per_sheet) {
  // Supports N-up: 1 2 4 6 9 16

  return pages_per_sheet == 1 || pages_per_sheet == 2 || pages_per_sheet == 4 ||
         pages_per_sheet == 6 || pages_per_sheet == 9 || pages_per_sheet == 16;
}

void NupParameters::SetParameters(int pages_per_sheet,
                                  bool is_source_landscape) {
  DCHECK(IsSupported(pages_per_sheet));

  switch (pages_per_sheet) {
    case 1:
      num_pages_on_x_axis_ = 1;
      num_pages_on_y_axis_ = 1;
      break;
    case 2:
      if (!is_source_landscape) {
        num_pages_on_x_axis_ = 2;
        num_pages_on_y_axis_ = 1;
        landscape_ = true;
      } else {
        num_pages_on_x_axis_ = 1;
        num_pages_on_y_axis_ = 2;
      }
      break;
    case 6:
      if (!is_source_landscape) {
        num_pages_on_x_axis_ = 3;
        num_pages_on_y_axis_ = 2;
        landscape_ = true;
      } else {
        num_pages_on_x_axis_ = 2;
        num_pages_on_y_axis_ = 3;
      }
      break;
    case 4:
    case 9:
    case 16:
      num_pages_on_x_axis_ = std::sqrt(pages_per_sheet);
      num_pages_on_y_axis_ = std::sqrt(pages_per_sheet);
      if (is_source_landscape)
        landscape_ = true;
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace printing
