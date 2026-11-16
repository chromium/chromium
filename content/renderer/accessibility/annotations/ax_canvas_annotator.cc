// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_canvas_annotator.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/skia_span_util.h"

namespace content {

namespace {

// Prune stale annotations after this many updates. This value was chosen
// arbitrarily and can be adjusted or A/B tested.
constexpr int kMaxUpdatesBeforePruning = 50;

constexpr int kMaxCanvasDimension = 2048;

std::optional<size_t> ComputeBitmapHash(const SkBitmap& bitmap) {
  // Guard against unallocated pixel buffers before creating a span, since
  // SkPixmap::computeByteSize() can be non-zero when getPixels() is null.
  if (bitmap.drawsNothing() || !bitmap.getPixels()) {
    return std::nullopt;
  }
  return base::FastHash(gfx::SkPixmapToSpan(bitmap.pixmap()));
}

}  // namespace

AXCanvasAnnotator::AXCanvasAnnotator(
    RenderAccessibilityImpl* const render_accessibility)
    : render_accessibility_(*render_accessibility) {}

AXCanvasAnnotator::~AXCanvasAnnotator() = default;

void AXCanvasAnnotator::Annotate(const blink::WebDocument& document,
                                 ui::AXTreeUpdate* update,
                                 bool load_complete) {
  if (!annotations_enabled_) {
    return;
  }

  if (current_document_ != document) {
    current_document_ = document;
    canvas_annotations_.clear();
  }

  // Prune stale annotations periodically to avoid memory bloat.
  if (load_complete ||
      ++updates_since_last_pruning_ >= kMaxUpdatesBeforePruning) {
    updates_since_last_pruning_ = 0;
    PruneStaleAnnotations(document);
  }

  for (auto& node_data : update->nodes) {
    if (node_data.role != ax::mojom::Role::kCanvas) {
      continue;
    }

    blink::WebAXObject src =
        blink::WebAXObject::FromWebDocumentByID(document, node_data.id);
    AddCanvasAnnotationForNode(src, node_data);
  }
}

void AXCanvasAnnotator::EnableAnnotations() {
  annotations_enabled_ = true;
}

void AXCanvasAnnotator::CancelAnnotations() {
  if (!annotations_enabled_) {
    return;
  }
  annotations_enabled_ = false;
  OnOcrDisconnected();
}

bool AXCanvasAnnotator::ConnectOcrIfNeeded() {
  // If remote is already bound and connected, reuse it. If bound but
  // disconnected (e.g., OCR service restarted or disconnected), falling through
  // to reset() safely resets the remote so it can be rebound.
  if (annotator_remote_.is_bound() && annotator_remote_.is_connected()) {
    return true;
  }
  if (!render_accessibility_->render_frame()) {
    return false;
  }
  annotator_remote_.reset();
  render_accessibility_->render_frame()
      ->GetBrowserInterfaceBroker()
      .GetInterface(annotator_remote_.BindNewPipeAndPassReceiver());
  annotator_remote_->SetClientType(screen_ai::mojom::OcrClientType::kCanvas);
  annotator_remote_.set_disconnect_handler(base::BindOnce(
      &AXCanvasAnnotator::OnOcrDisconnected, weak_factory_.GetWeakPtr()));
  return true;
}

void AXCanvasAnnotator::OnOcrDisconnected() {
  annotator_remote_.reset();
  for (auto& [ax_id, info] : canvas_annotations_) {
    if (info.is_pending) {
      info.is_pending = false;
      // Reset the cached bitmap hash so that if the canvas still needs OCR,
      // it can be re-evaluated when the service reconnects.
      info.last_bitmap_hash.reset();
    }
  }
}

uint32_t AXCanvasAnnotator::GetAXModeToEnableAnnotations() {
  return ui::AXMode::kScreenReader;
}

bool AXCanvasAnnotator::HasAXActionToEnableAnnotations() {
  return false;
}

ax::mojom::Action AXCanvasAnnotator::GetAXActionToEnableAnnotations() {
  return ax::mojom::Action::kNone;
}

void AXCanvasAnnotator::AddDebuggingAttributes(
    const std::vector<ui::AXTreeUpdate>& updates) {
  DVLOG(1) << "AXCanvasAnnotator::AddDebuggingAttributes - "
              "Canvas annotations size: "
           << canvas_annotations_.size();
  for (const auto& [ax_id, info] : canvas_annotations_) {
    DVLOG(1) << "  AXID: " << ax_id
             << ", is_pending: " << (info.is_pending ? "true" : "false")
             << ", text: \"" << info.text << "\"";
  }
}

void AXCanvasAnnotator::AddCanvasAnnotationForNode(blink::WebAXObject& src,
                                                   ui::AXNodeData& dst) {
  if (src.IsIgnored() || src.IsDetached()) {
    return;
  }

  RefreshCanvasAnnotation(src);

  if (const auto* info = base::FindOrNull(canvas_annotations_, src.AxID());
      info && !info->text.empty()) {
    // Preserve existing OCR annotation text when no new OCR pass is needed
    // or while a new OCR request runs asynchronously.
    dst.AddStringAttribute(ax::mojom::StringAttribute::kCanvasAnnotation,
                           info->text);
  }
}

void AXCanvasAnnotator::RefreshCanvasAnnotation(blink::WebAXObject& src) {
  // Do not initiate image extraction if OCR was not requested for this canvas.
  if (!src.HasRequestedOCR()) {
    return;
  }

  blink::WebNode node = src.GetNode();
  if (node.IsNull() || !node.IsElementNode()) {
    return;
  }

  // Wait for any in-flight OCR request to complete before taking a screenshot;
  // OnCanvasAnnotated() triggers a follow-up serialization pass that will
  // process the latest canvas state.
  CanvasAnnotationInfo& info = canvas_annotations_[src.AxID()];
  if (info.is_pending) {
    return;
  }

  SkBitmap bitmap = node.To<blink::WebElement>().ImageContents();
  std::optional<size_t> bitmap_hash = ComputeBitmapHash(bitmap);
  if (!bitmap_hash.has_value()) {
    // Clear cached OCR text when the screenshot is empty, while leaving `dst`
    // untouched so any 2D text runs Blink serialized onto `dst` stay intact.
    src.ClearHasRequestedOCR();
    SetCanvasAnnotation(info, std::string());
    info.last_bitmap_hash.reset();
    return;
  }

  // Skip OCR if the pixel contents match the last frame dispatched for OCR.
  if (bitmap_hash == info.last_bitmap_hash) {
    src.ClearHasRequestedOCR();
    return;
  }

  // The OCR service Mojo interface requires skia.mojom.BitmapN32, which
  // enforces kN32_SkColorType and tightly packed rowBytes (rowBytes ==
  // minRowBytes). Convert non-N32 bitmaps (e.g. float16 wide-gamut canvases)
  // and repack bitmaps with row padding into tightly packed N32 format.
  if (bitmap.colorType() != kN32_SkColorType ||
      bitmap.rowBytes() != bitmap.info().minRowBytes()) {
    SkBitmap converted;
    if (converted.tryAllocPixels(
            SkImageInfo::MakeN32Premul(bitmap.width(), bitmap.height())) &&
        bitmap.readPixels(converted.pixmap())) {
      bitmap = std::move(converted);
    } else {
      // Clear the request flag so failed pixel conversion does not trigger
      // repeated canvas readbacks on subsequent accessibility updates.
      src.ClearHasRequestedOCR();
      return;
    }
  }

  // Proportionally downsample large canvases (e.g. on High-DPI / Retina
  // displays) to fit within kMaxCanvasDimension instead of dropping them.
  // RESIZE_BETTER is used to avoid main-thread UI stalls from Lanczos-3.
  if (bitmap.width() > kMaxCanvasDimension ||
      bitmap.height() > kMaxCanvasDimension) {
    const float scale =
        std::min(static_cast<float>(kMaxCanvasDimension) / bitmap.width(),
                 static_cast<float>(kMaxCanvasDimension) / bitmap.height());
    const int new_width =
        std::clamp(static_cast<int>(std::round(bitmap.width() * scale)), 1,
                   kMaxCanvasDimension);
    const int new_height =
        std::clamp(static_cast<int>(std::round(bitmap.height() * scale)), 1,
                   kMaxCanvasDimension);
    bitmap = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_BETTER, new_width, new_height);
    if (bitmap.drawsNothing()) {
      src.ClearHasRequestedOCR();
      return;
    }
  }

  if (!ConnectOcrIfNeeded()) {
    // Keep HasRequestedOCR() set so a later update retries once connected.
    return;
  }

  // Only clear the OCR requested state on the source node once the bitmap and
  // OCR service connection have succeeded and the request is being dispatched.
  src.ClearHasRequestedOCR();

  info.is_pending = true;
  info.last_bitmap_hash = bitmap_hash;

  annotator_remote_->PerformOcrAndReturnAnnotation(
      bitmap, base::BindOnce(&AXCanvasAnnotator::OnCanvasAnnotated,
                             weak_factory_.GetWeakPtr(), src));
}

void AXCanvasAnnotator::SetCanvasAnnotation(CanvasAnnotationInfo& info,
                                            std::string text) {
  info.text = std::move(text);
  info.is_pending = false;
}

void AXCanvasAnnotator::OnCanvasAnnotated(
    const blink::WebAXObject& canvas,
    screen_ai::mojom::VisualAnnotationPtr visual_annotation) {
  if (canvas.IsDetached()) {
    canvas_annotations_.erase(canvas.AxID());
    return;
  }

  // TODO(crbug.com/498093320): Use OCR blocks and paragraphs to better
  // represent the structure of drawn text.
  std::string text;
  if (visual_annotation) {
    size_t total_size = 0;
    for (const auto& line : visual_annotation->lines) {
      total_size += line->text_line.size() + 1;
    }
    text.reserve(total_size);
    for (const auto& line : visual_annotation->lines) {
      if (!text.empty()) {
        text += "\n";
      }
      text += line->text_line;
    }
  }

  auto it = canvas_annotations_.find(canvas.AxID());
  if (it == canvas_annotations_.end()) {
    return;
  }
  SetCanvasAnnotation(it->second, std::move(text));

  // Mark the canvas object dirty and fire kChildrenChanged with immediate
  // serialization. Unlike static images, canvas OCR annotations update
  // dynamically and require immediate IPC dispatch to update the browser-side
  // tree snapshot, as well as kChildrenChanged notifications for screen
  // readers.
  render_accessibility_->MarkWebAXObjectDirty(canvas);
  render_accessibility_->HandleAXEvent(
      ui::AXEvent(canvas.AxID(), ax::mojom::Event::kChildrenChanged));
}

void AXCanvasAnnotator::PruneStaleAnnotations(
    const blink::WebDocument& document) {
  // TODO(crbug.com/498093320): Add metrics on cache size and cleanup and
  // consider storing the annotations directly in Blink's HTMLCanvasElement to
  // avoid the need for a cache.
  std::erase_if(canvas_annotations_, [&document](const auto& item) {
    blink::WebAXObject obj =
        blink::WebAXObject::FromWebDocumentByID(document, item.first);
    return obj.IsNull() || obj.IsDetached();
  });
}

}  // namespace content
