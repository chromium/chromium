// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_canvas_annotator.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_ax_context.h"
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

constexpr int kMaxCanvasDimension = 2048;

std::optional<size_t> ComputeBitmapHash(const SkBitmap& bitmap) {
  // Guard against unallocated pixel buffers before creating a span, since
  // SkPixmap::computeByteSize() can be non-zero when getPixels() is null.
  if (bitmap.drawsNothing() || !bitmap.getPixels()) {
    return std::nullopt;
  }
  return base::FastHash(gfx::SkPixmapToSpan(bitmap.pixmap()));
}

// Extracts newline-separated non-empty text lines from `visual_annotation`.
// TODO(crbug.com/498093320): Use OCR blocks and paragraphs to better
// represent the structure of drawn text.
std::string ExtractOcrText(
    const screen_ai::mojom::VisualAnnotation& visual_annotation) {
  size_t total_size = 0;
  for (const auto& line : visual_annotation.lines) {
    if (!line->text_line.empty()) {
      total_size += line->text_line.size() + 1;
    }
  }
  std::string text;
  text.reserve(total_size);
  for (const auto& line : visual_annotation.lines) {
    if (line->text_line.empty()) {
      continue;
    }
    if (!text.empty()) {
      text += "\n";
    }
    text += line->text_line;
  }
  return text;
}

}  // namespace

AXCanvasAnnotator::AXCanvasAnnotator(
    RenderAccessibilityImpl* const render_accessibility)
    : render_accessibility_(CHECK_DEREF(render_accessibility)) {}

AXCanvasAnnotator::~AXCanvasAnnotator() = default;

void AXCanvasAnnotator::Annotate(const blink::WebDocument& document,
                                 ui::AXTreeUpdate* update,
                                 bool load_complete) {
  if (!annotations_enabled_) {
    return;
  }

  PruneStaleAnnotations(document);

  for (auto& node_data : update->nodes) {
    if (node_data.role != ax::mojom::Role::kCanvas) {
      continue;
    }

    blink::WebAXObject src =
        blink::WebAXObject::FromWebDocumentByID(document, node_data.id);
    AddCanvasAnnotationForNode(src, node_data);
  }
}

void AXCanvasAnnotator::DidCreateNewDocument() {
  annotations_enabled_ = false;
  annotator_remote_.reset();
  canvas_annotations_.clear();
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
  return kEnableAXMode;
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

void AXCanvasAnnotator::AddCanvasAnnotationForNode(
    const blink::WebAXObject& src,
    ui::AXNodeData& dst) {
  if (src.IsDetached() || src.IsIgnored()) {
    // Skip ignored canvases without evicting cached OCR text so hidden static
    // canvases keep their annotation when shown again.
    return;
  }

  RefreshCanvasAnnotation(src, dst.id);

  if (const auto* info = base::FindOrNull(canvas_annotations_, dst.id);
      info && !info->text.empty()) {
    // Preserve existing OCR annotation text when no new OCR pass is needed
    // or while a new OCR request runs asynchronously.
    dst.AddStringAttribute(ax::mojom::StringAttribute::kCanvasAnnotation,
                           info->text);
  }
}

void AXCanvasAnnotator::RefreshCanvasAnnotation(const blink::WebAXObject& src,
                                                int ax_id) {
  CHECK(!src.IsDetached());
  CHECK(!src.IsIgnored());

  // Do not initiate image extraction if OCR was not requested for this canvas.
  if (!src.HasRequestedOCR()) {
    return;
  }

  // Wait for any in-flight OCR request to complete before taking a screenshot;
  // OnCanvasAnnotated() triggers a follow-up serialization pass that will
  // process the latest canvas state.
  CanvasAnnotationInfo& info = canvas_annotations_[ax_id];
  if (info.is_pending) {
    return;
  }

  blink::WebElement element = src.GetNode().To<blink::WebElement>();
  CHECK(!element.IsNull());
  SkBitmap bitmap = element.ImageContents();
  std::optional<size_t> bitmap_hash = ComputeBitmapHash(bitmap);
  if (!bitmap_hash.has_value()) {
    // Clear cached OCR text when the screenshot is empty, while leaving `dst`
    // untouched so any 2D text runs Blink serialized onto `dst` stay intact.
    EvictCanvasAnnotation(src, ax_id);
    return;
  }

  // Skip OCR if the pixel contents match the last frame dispatched for OCR.
  if (bitmap_hash == info.last_bitmap_hash) {
    KeepCachedCanvasAnnotation(src);
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
      // Evict cached state and clear the request flag so failed pixel
      // conversion does not leave stale text or trigger repeated readbacks.
      EvictCanvasAnnotation(src, ax_id);
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
      EvictCanvasAnnotation(src, ax_id);
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
                             weak_factory_.GetWeakPtr(), ax_id));
}

void AXCanvasAnnotator::EvictCanvasAnnotation(const blink::WebAXObject& src,
                                              int ax_id) {
  src.ClearHasRequestedOCR();
  canvas_annotations_.erase(ax_id);
}

void AXCanvasAnnotator::KeepCachedCanvasAnnotation(
    const blink::WebAXObject& src) {
  // Keep `text` and `last_bitmap_hash` in `canvas_annotations_` and clear
  // Blink's request flag since the pixel contents have not changed.
  src.ClearHasRequestedOCR();
}

void AXCanvasAnnotator::SetCanvasAnnotation(CanvasAnnotationInfo& info,
                                            std::string text) {
  info.text = std::move(text);
  info.is_pending = false;
}

void AXCanvasAnnotator::OnCanvasAnnotated(
    int ax_id,
    screen_ai::mojom::VisualAnnotationPtr visual_annotation) {
  if (!FindPendingAnnotation(ax_id)) {
    return;
  }

  blink::WebAXContext* ax_context = render_accessibility_->GetAXContext();
  CHECK(ax_context);
  if (!ax_context->HasActiveDocument()) {
    return;
  }

  // Updating layout and AX across all documents brings the lifecycle to
  // kLayoutClean before MarkWebAXObjectDirty() checks IsIncludedInTree(). It
  // can also synchronously serialize (re-entering Annotate() and
  // PruneStaleAnnotations()) or run observers that destroy this annotator or
  // replace `ax_context_`.
  // TODO(crbug.com/498093320): Replace synchronous UpdateAXForAllDocuments()
  // with an asynchronous post-layout callback (e.g. RunAfterAXLayoutClean())
  // once available.
  base::WeakPtr<AXCanvasAnnotator> weak_this = weak_factory_.GetWeakPtr();
  ax_context->UpdateAXForAllDocuments();
  if (!weak_this) {
    return;
  }
  ax_context = render_accessibility_->GetAXContext();
  CHECK(ax_context);
  if (!ax_context->HasActiveDocument()) {
    return;
  }

  ApplyCanvasAnnotation(ax_id, *ax_context, *visual_annotation);
}

void AXCanvasAnnotator::ApplyCanvasAnnotation(
    int ax_id,
    blink::WebAXContext& ax_context,
    const screen_ai::mojom::VisualAnnotation& visual_annotation) {
  CanvasAnnotationInfo* info = FindPendingAnnotation(ax_id);
  if (!info) {
    return;
  }

  // Resolve the live WebAXObject by `ax_id` rather than binding a WebAXObject
  // directly, since WebAXObject::AxID() returns -1 once detached and would fail
  // to erase the pending entry if the AXObject was detached or recreated.
  blink::WebAXObject canvas = blink::WebAXObject::FromWebDocumentByID(
      render_accessibility_->GetMainDocument(), ax_id);
  if (canvas.IsDetached()) {
    EvictCanvasAnnotation(canvas, ax_id);
    return;
  }

  std::string text = ExtractOcrText(visual_annotation);
  const bool text_changed = info->text != text;
  SetCanvasAnnotation(*info, std::move(text));

  // Dirty the canvas and schedule immediate serialization when the annotation
  // text changed or when another frame was queued while this request was in
  // flight (`canvas.HasRequestedOCR()`).
  if (text_changed || canvas.HasRequestedOCR()) {
    MarkCanvasDirty(canvas, ax_context);
  }
}

AXCanvasAnnotator::CanvasAnnotationInfo*
AXCanvasAnnotator::FindPendingAnnotation(int ax_id) {
  auto* info = base::FindOrNull(canvas_annotations_, ax_id);
  if (!info || !info->is_pending) {
    return nullptr;
  }
  // Disabling or resetting annotations always clears `is_pending` across the
  // cache, so a pending entry can only exist while annotations are enabled.
  CHECK(annotations_enabled_);
  return info;
}

void AXCanvasAnnotator::MarkCanvasDirty(const blink::WebAXObject& canvas,
                                        blink::WebAXContext& ax_context) {
  if (canvas.IsIncludedInTree()) {
    render_accessibility_->MarkWebAXObjectDirty(canvas);
    ax_context.ScheduleImmediateSerialization();
  }
}

void AXCanvasAnnotator::PruneStaleAnnotations(
    const blink::WebDocument& document) {
  // TODO(crbug.com/498093320): Add metrics on cache size and cleanup and
  // consider storing the annotations directly in Blink's HTMLCanvasElement to
  // avoid the need for a cache.
  std::erase_if(canvas_annotations_, [&document](const auto& item) {
    return blink::WebAXObject::FromWebDocumentByID(document, item.first)
        .IsDetached();
  });
}

}  // namespace content
