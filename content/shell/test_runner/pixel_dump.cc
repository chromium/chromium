// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/pixel_dump.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "content/shell/common/web_test/web_test_utils.h"
#include "content/shell/test_runner/web_test_runtime_flags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "services/service_manager/public/cpp/connector.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/gfx/geometry/point.h"

namespace test_runner {

namespace {

void CapturePixelsForPrinting(
    blink::WebLocalFrame* web_frame,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  auto* frame_widget = web_frame->LocalRoot()->FrameWidget();
  frame_widget->UpdateAllLifecyclePhases(
      blink::WebWidget::LifecycleUpdateReason::kTest);

  blink::WebSize page_size_in_pixels = frame_widget->Size();

  int page_count = web_frame->PrintBegin(page_size_in_pixels);
  int total_height = page_count * (page_size_in_pixels.height + 1) - 1;

  bool is_opaque = false;

  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(page_size_in_pixels.width, total_height,
                                is_opaque)) {
    LOG(ERROR) << "Failed to create bitmap width=" << page_size_in_pixels.width
               << " height=" << total_height;
    std::move(callback).Run(SkBitmap());
    return;
  }

  printing::MetafileSkia metafile(printing::SkiaDocumentType::MSKP,
                                  printing::PrintSettings::NewCookie());
  cc::SkiaPaintCanvas canvas(bitmap);
  canvas.SetPrintingMetafile(&metafile);
  web_frame->PrintPagesForTesting(&canvas, page_size_in_pixels);
  web_frame->PrintEnd();

  std::move(callback).Run(bitmap);
}

}  // namespace

void PrintFrameAsync(blink::WebLocalFrame* web_frame,
                     base::OnceCallback<void(const SkBitmap&)> callback) {
  DCHECK(web_frame);
  DCHECK(callback);
  web_frame->GetTaskRunner(blink::TaskType::kInternalTest)
      ->PostTask(FROM_HERE, base::BindOnce(&CapturePixelsForPrinting,
                                           base::Unretained(web_frame),
                                           std::move(callback)));
}

void CopyImageAtAndCapturePixels(
    blink::WebLocalFrame* web_frame,
    int x,
    int y,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  mojo::Remote<blink::mojom::ClipboardHost> clipboard;
  blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      clipboard.BindNewPipeAndPassReceiver());

  uint64_t sequence_number_before = 0;
  clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste,
                               &sequence_number_before);
  web_frame->CopyImageAt(blink::WebPoint(x, y));
  uint64_t sequence_number_after = 0;
  while (sequence_number_before == sequence_number_after) {
    clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste,
                                 &sequence_number_after);
  }

  SkBitmap bitmap;
  clipboard->ReadImage(ui::ClipboardBuffer::kCopyPaste, &bitmap);
  std::move(callback).Run(bitmap);
}

}  // namespace test_runner
