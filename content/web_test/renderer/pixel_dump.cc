// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/pixel_dump.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "content/public/renderer/render_frame.h"
#include "content/web_test/common/web_test_runtime_flags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_number.h"
#include "printing/page_range.h"
#include "printing/print_settings.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

SkBitmap PrintFrameToBitmap(blink::WebLocalFrame* web_frame,
                            const gfx::Size& page_size_in_pixels,
                            const printing::PageRanges& page_ranges) {
  auto* frame_widget = web_frame->LocalRoot()->FrameWidget();
  frame_widget->UpdateAllLifecyclePhases(blink::DocumentUpdateReason::kTest);

  gfx::SizeF used_page_size(page_size_in_pixels);

  uint32_t page_count = web_frame->PrintBegin(
      blink::WebPrintParams(used_page_size), blink::WebNode());

  // Check the desired size of the first page, according to Blink, and use that
  // as the actual size for all pages. This is similar to what regular Chrome
  // printing (PrintRenderFrameHelper & co) does, in order to honor @page CSS
  // rules. This will need to change when adding support for mixed page
  // sizes. See crbug.com/835358
  blink::WebPrintPageDescription description;
  description.size = used_page_size;
  web_frame->GetPageDescription(0, &description);
  gfx::SizeF first_page_area_size(
      description.size.width() -
          (description.margin_left + description.margin_right),
      description.size.height() -
          (description.margin_top + description.margin_bottom));

  if (used_page_size != first_page_area_size &&
      first_page_area_size.width() >= 1 && first_page_area_size.height() >= 1) {
    // A valid and different page size has been specified in CSS. Relayout.
    web_frame->PrintEnd();
    used_page_size = first_page_area_size;
    page_count = web_frame->PrintBegin(blink::WebPrintParams(used_page_size),
                                       blink::WebNode());
  }

  blink::WebVector<uint32_t> pages(
      printing::PageNumber::GetPages(page_ranges, page_count));
  gfx::Size spool_size = web_frame->SpoolSizeInPixelsForTesting(
      gfx::ToFlooredSize(used_page_size), pages);

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
  web_frame->PrintPagesForTesting(&canvas, gfx::ToFlooredSize(used_page_size),
                                  spool_size, &pages);
  web_frame->PrintEnd();
  return bitmap;
}

}  // namespace content
