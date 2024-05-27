// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/pixel_dump.h"

#include "base/logging.h"
#include "cc/paint/skia_paint_canvas.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_number.h"
#include "printing/page_range.h"
#include "printing/print_settings.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "ui/gfx/geometry/size.h"

namespace content {

SkBitmap PrintFrameToBitmap(blink::WebLocalFrame* web_frame,
                            const blink::WebPrintParams& print_params,
                            const printing::PageRanges& page_ranges) {
  auto* frame_widget = web_frame->LocalRoot()->FrameWidget();
  frame_widget->UpdateAllLifecyclePhases(blink::DocumentUpdateReason::kTest);

  uint32_t page_count = web_frame->PrintBegin(print_params, blink::WebNode());

  blink::WebVector<uint32_t> pages(
      printing::PageNumber::GetPages(page_ranges, page_count));
  gfx::Size spool_size = web_frame->SpoolSizeInPixelsForTesting(pages);

  bool is_opaque = false;

  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(spool_size.width(), spool_size.height(),
                                is_opaque)) {
    LOG(ERROR) << "Failed to create bitmap width=" << spool_size.width()
               << " height=" << spool_size.height();
    return SkBitmap();
  }

  printing::MetafileSkia metafile(printing::mojom::SkiaDocumentType::kMSKP,
                                  printing::PrintSettings::NewCookie());
  cc::SkiaPaintCanvas canvas(bitmap);
  canvas.SetPrintingMetafile(&metafile);
  web_frame->PrintPagesForTesting(&canvas, spool_size, &pages);
  web_frame->PrintEnd();
  return bitmap;
}

}  // namespace content
