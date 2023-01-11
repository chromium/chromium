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
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/gfx/geometry/point.h"

namespace content {

SkBitmap PrintFrameToBitmap(blink::WebLocalFrame* web_frame,
                            const gfx::Size& page_size_in_pixels,
                            const printing::PageRanges& page_ranges) {
  auto* frame_widget = web_frame->LocalRoot()->FrameWidget();
  frame_widget->UpdateAllLifecyclePhases(blink::DocumentUpdateReason::kTest);

  uint32_t page_count =
      web_frame->PrintBegin(page_size_in_pixels, blink::WebNode());

  blink::WebVector<uint32_t> pages(
      printing::PageNumber::GetPages(page_ranges, page_count));
  gfx::Size spool_size =
      web_frame->SpoolSizeInPixelsForTesting(page_size_in_pixels, pages);

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
  web_frame->PrintPagesForTesting(&canvas, page_size_in_pixels, spool_size,
                                  &pages);
  web_frame->PrintEnd();
  return bitmap;
}

}  // namespace content
