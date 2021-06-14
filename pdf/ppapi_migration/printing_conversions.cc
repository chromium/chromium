// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/printing_conversions.h"

#include <stdint.h>

#include <vector>

#include "base/check.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "ppapi/c/dev/pp_print_settings_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/blink/public/web/web_print_params.h"

namespace chrome_pdf {

std::vector<int> PageNumbersFromPPPrintPageNumberRange(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count) {
  DCHECK(page_range_count);

  std::vector<int> page_numbers;
  for (uint32_t i = 0; i < page_range_count; ++i) {
    for (uint32_t page_number = page_ranges[i].first_page_number;
         page_number <= page_ranges[i].last_page_number; ++page_number) {
      page_numbers.push_back(page_number);
    }
  }

  return page_numbers;
}

blink::WebPrintParams WebPrintParamsFromPPPrintSettings(
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings) {
  blink::WebPrintParams params;
  params.print_content_area = RectFromPPRect(print_settings.content_area);
  params.printable_area = RectFromPPRect(print_settings.printable_area);
  params.paper_size = SizeFromPPSize(print_settings.paper_size);
  params.printer_dpi = print_settings.dpi;
  params.scale_factor = pdf_print_settings.scale_factor;
  params.rasterize_pdf = print_settings.format & PP_PRINTOUTPUTFORMAT_RASTER;
  params.print_scaling_option =
      static_cast<printing::mojom::PrintScalingOption>(
          print_settings.print_scaling_option);
  params.pages_per_sheet = pdf_print_settings.pages_per_sheet;
  return params;
}

}  // namespace chrome_pdf
