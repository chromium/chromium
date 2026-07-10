// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_canvas_annotator.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_update.h"

namespace content {

namespace {

// Prune stale annotations after this many updates. This value was chosen
// arbitrarily and can be adjusted or A/B tested.
constexpr int kMaxUpdatesBeforePruning = 50;

constexpr int kMaxCanvasDimension = 2048;

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

void AXCanvasAnnotator::ConnectOcrIfNeeded() {
  if (annotator_remote_.is_connected()) {
    return;
  }
  if (!render_accessibility_->render_frame()) {
    return;
  }
  annotator_remote_.reset();
  render_accessibility_->render_frame()
      ->GetBrowserInterfaceBroker()
      .GetInterface(annotator_remote_.BindNewPipeAndPassReceiver());
  annotator_remote_->SetClientType(screen_ai::mojom::OcrClientType::kCanvas);
  annotator_remote_.set_disconnect_handler(base::BindOnce(
      &AXCanvasAnnotator::OnOcrDisconnected, weak_factory_.GetWeakPtr()));
}

void AXCanvasAnnotator::OnOcrDisconnected() {
  annotator_remote_.reset();
  std::erase_if(canvas_annotations_,
                [](const auto& item) { return item.second.is_pending; });
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

  bool ocr_requested = src.HasRequestedOCR();
  if (ocr_requested) {
    // If a request is already in flight, starting a new one is safe; Mojo
    // guarantees in-order response delivery, so the latest request's output
    // will always overwrite any stale responses when it arrives. OCR service
    // does not support canceling a request.
    canvas_annotations_.erase(src.AxID());
    src.ClearHasRequestedOCR();
  }

  auto it = canvas_annotations_.find(src.AxID());
  if (it != canvas_annotations_.end()) {
    if (it->second.is_pending) {
      return;
    }
    if (!it->second.text.empty()) {
      dst.AddStringAttribute(ax::mojom::StringAttribute::kCanvasAnnotation,
                             it->second.text);
    }
    return;
  }

  // Do not initiate image extraction if OCR has not been requested.
  if (!ocr_requested) {
    return;
  }

  blink::WebNode node = src.GetNode();
  if (node.IsNull() || !node.IsElementNode()) {
    return;
  }

  blink::WebElement element = node.To<blink::WebElement>();
  SkBitmap bitmap = element.ImageContents();

  // Drop processing for massive canvases to avoid OOM.
  // TODO(crbug.com/498093320): Consider high-quality downsampling for large
  // canvases in the future instead of dropping them completely.
  bool is_too_large = bitmap.width() > kMaxCanvasDimension ||
                      bitmap.height() > kMaxCanvasDimension;
  if (is_too_large || bitmap.drawsNothing()) {
    canvas_annotations_[src.AxID()] = {.text = "", .is_pending = false};
    return;
  }

  ConnectOcrIfNeeded();
  if (!annotator_remote_.is_connected()) {
    return;
  }

  canvas_annotations_[src.AxID()] = {.text = "", .is_pending = true};

  annotator_remote_->PerformOcrAndReturnAnnotation(
      bitmap, base::BindOnce(&AXCanvasAnnotator::OnCanvasAnnotated,
                             weak_factory_.GetWeakPtr(), src));
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

  canvas_annotations_[canvas.AxID()] = {.text = std::move(text),
                                        .is_pending = false};
  render_accessibility_->MarkWebAXObjectDirty(canvas);
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
