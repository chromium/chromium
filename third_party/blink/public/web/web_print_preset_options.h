// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PRINT_PRESET_OPTIONS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PRINT_PRESET_OPTIONS_H_

#include <vector>

#include "printing/mojom/print.mojom-shared.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

struct WebPageRange;
typedef std::vector<WebPageRange> WebPageRanges;

struct WebPageRange {
  int from;
  int to;
};

struct WebPrintPresetOptions {
  WebPrintPresetOptions()
      : is_scaling_disabled(false),
        copies(0),
        duplex_mode(printing::mojom::DuplexMode::kUnknownDuplexMode) {}

  // Specifies whether scaling is disabled.
  bool is_scaling_disabled;

  // Specifies the number of copies to be printed.
  int copies;

  // Specifies duplex mode to be used for printing.
  printing::mojom::DuplexMode duplex_mode;

  // Specifies page range to be used for printing.
  WebPageRanges page_ranges;

  // True if all the pages in the PDF are the same size.
  bool is_page_size_uniform;

  // Only valid if the page sizes are uniform. The page size in points.
  gfx::Size uniform_page_size;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PRINT_PRESET_OPTIONS_H_
