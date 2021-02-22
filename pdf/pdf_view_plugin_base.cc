// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/accessibility.h"
#include "pdf/accessibility_structs.h"
#include "pdf/paint_ready_rect.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/ppapi_migration/image.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

namespace chrome_pdf {

namespace {

// The minimum zoom level allowed.
constexpr double kMinZoom = 0.01;

// A delay to wait between each accessibility page to keep the system
// responsive.
constexpr base::TimeDelta kAccessibilityPageDelay =
    base::TimeDelta::FromMilliseconds(100);

// Enumeration of pinch states.
// This should match PinchPhase enum in chrome/browser/resources/pdf/viewport.js
enum class PinchPhase {
  kNone = 0,
  kStart = 1,
  kUpdateZoomOut = 2,
  kUpdateZoomIn = 3,
  kEnd = 4,
};

// Prepares messages from the plugin that reply to messages from the embedder.
// If the "type" value of `message` is "foo", then the `reply_type` must be
// "fooReply". The `message` from the embedder must have a "messageId" value
// that will be copied to the reply message.
base::Value PrepareReplyMessage(base::StringPiece reply_type,
                                const base::Value& message) {
  DCHECK_EQ(reply_type, *message.FindStringKey("type") + "Reply");

  const std::string* message_id = message.FindStringKey("messageId");
  CHECK(message_id);

  base::Value reply(base::Value::Type::DICTIONARY);
  reply.SetStringKey("type", reply_type);
  reply.SetStringKey("messageId", *message_id);
  return reply;
}

}  // namespace

PdfViewPluginBase::PdfViewPluginBase() = default;

PdfViewPluginBase::~PdfViewPluginBase() = default;

void PdfViewPluginBase::Invalidate(const gfx::Rect& rect) {
  if (in_paint_) {
    deferred_invalidates_.push_back(rect);
    return;
  }

  gfx::Rect offset_rect = rect + available_area_.OffsetFromOrigin();
  paint_manager_.InvalidateRect(offset_rect);
}

SkColor PdfViewPluginBase::GetBackgroundColor() {
  return background_color_;
}

void PdfViewPluginBase::HandleMessage(const base::Value& message) {
  using MessageHandler = void (PdfViewPluginBase::*)(const base::Value&);
  static constexpr auto kMessageHandlers =
      base::MakeFixedFlatMap<base::StringPiece, MessageHandler>({
          {"displayAnnotations",
           &PdfViewPluginBase::HandleDisplayAnnotationsMessage},
          {"getNamedDestination",
           &PdfViewPluginBase::HandleGetNamedDestinationMessage},
          {"getSelectedText", &PdfViewPluginBase::HandleGetSelectedTextMessage},
          {"rotateClockwise", &PdfViewPluginBase::HandleRotateClockwiseMessage},
          {"rotateCounterclockwise",
           &PdfViewPluginBase::HandleRotateCounterclockwiseMessage},
          {"selectAll", &PdfViewPluginBase::HandleSelectAllMessage},
          {"setBackgroundColor",
           &PdfViewPluginBase::HandleSetBackgroundColorMessage},
          {"setReadOnly", &PdfViewPluginBase::HandleSetReadOnlyMessage},
          {"setTwoUpView", &PdfViewPluginBase::HandleSetTwoUpViewMessage},
          {"viewport", &PdfViewPluginBase::HandleViewportMessage},
      });

  const std::string* type = message.FindStringKey("type");
  CHECK(type);

  // TODO(crbug.com/1109796): Use `fixed_flat_map<>::at()` when migration is
  // complete to CHECK out-of-bounds lookups.
  const auto* it = kMessageHandlers.find(*type);
  if (it == kMessageHandlers.end()) {
    NOTIMPLEMENTED() << message;
    return;
  }

  MessageHandler handler = it->second;
  (this->*handler)(message);
}

void PdfViewPluginBase::OnPaint(const std::vector<gfx::Rect>& paint_rects,
                                std::vector<PaintReadyRect>& ready,
                                std::vector<gfx::Rect>& pending) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  DoPaint(paint_rects, ready, pending);
}

void PdfViewPluginBase::EnableAccessibility() {
  if (accessibility_state_ == AccessibilityState::kLoaded)
    return;

  if (accessibility_state_ == AccessibilityState::kOff)
    accessibility_state_ = AccessibilityState::kPending;

  if (document_load_state_ == DocumentLoadState::kComplete)
    LoadAccessibility();
}

void PdfViewPluginBase::HandleAccessibilityAction(
    const AccessibilityActionData& action_data) {
  engine_->HandleAccessibilityAction(action_data);
}

void PdfViewPluginBase::InitializeEngine(
    PDFiumFormFiller::ScriptOption script_option) {
  engine_ = std::make_unique<PDFiumEngine>(this, script_option);
}

void PdfViewPluginBase::DestroyEngine() {
  engine_.reset();
}

void PdfViewPluginBase::LoadUrl(const std::string& url, bool is_print_preview) {
  UrlRequest request;
  request.url = url;
  request.method = "GET";
  request.ignore_redirects = true;

  std::unique_ptr<UrlLoader> loader = CreateUrlLoaderInternal();
  UrlLoader* raw_loader = loader.get();
  raw_loader->Open(
      request,
      base::BindOnce(is_print_preview ? &PdfViewPluginBase::DidOpenPreview
                                      : &PdfViewPluginBase::DidOpen,
                     GetWeakPtr(), std::move(loader)));
}

void PdfViewPluginBase::InvalidateAfterPaintDone() {
  if (deferred_invalidates_.empty())
    return;

  ScheduleTaskOnMainThread(
      base::TimeDelta(),
      base::BindOnce(&PdfViewPluginBase::ClearDeferredInvalidates,
                     GetWeakPtr()),
      0);
}

void PdfViewPluginBase::OnGeometryChanged(double old_zoom,
                                          float old_device_scale) {
  RecalculateAreas(old_zoom, old_device_scale);

  if (accessibility_state_ == AccessibilityState::kLoaded)
    PrepareAndSetAccessibilityViewportInfo();
}

void PdfViewPluginBase::UpdateGeometryOnViewChanged(
    const gfx::Rect& new_view_rect,
    float new_device_scale) {
  const gfx::Size view_device_size(new_view_rect.width() * new_device_scale,
                                   new_view_rect.height() * new_device_scale);

  if (view_device_size == plugin_size_ && new_device_scale == device_scale_ &&
      new_view_rect.origin() == plugin_offset_) {
    return;
  }

  const float old_device_scale = device_scale_;
  device_scale_ = new_device_scale;
  plugin_dip_size_ = new_view_rect.size();
  plugin_size_ = view_device_size;
  plugin_offset_ = new_view_rect.origin();

  paint_manager().SetSize(plugin_size_, device_scale_);

  // Initialize the image data buffer if the context size changes.
  const gfx::Size old_image_size = gfx::SkISizeToSize(image_data_.dimensions());
  const gfx::Size new_image_size =
      PaintManager::GetNewContextSize(old_image_size, plugin_size_);
  if (new_image_size != old_image_size) {
    InitImageData(new_image_size);
    first_paint_ = true;
  }

  // Skip updating the geometry if the new image data buffer is empty.
  if (image_data_.drawsNothing()) {
    DCHECK(plugin_size_.IsEmpty());
    return;
  }

  OnGeometryChanged(zoom_, old_device_scale);
}

Image PdfViewPluginBase::GetPluginImageData() const {
  return Image(image_data_);
}

void PdfViewPluginBase::RecalculateAreas(double old_zoom,
                                         float old_device_scale) {
  if (zoom_ != old_zoom || device_scale_ != old_device_scale)
    engine()->ZoomUpdated(zoom_ * device_scale_);

  available_area_ = gfx::Rect(plugin_size_);
  int doc_width = GetDocumentPixelWidth();
  if (doc_width < available_area_.width()) {
    // Center the document horizontally inside the plugin rectangle.
    available_area_.Offset((plugin_size_.width() - doc_width) / 2, 0);
    available_area_.set_width(doc_width);
  }

  // The distance between top of the plugin and the bottom of the document in
  // pixels.
  int bottom_of_document = GetDocumentPixelHeight();
  if (bottom_of_document < plugin_size_.height())
    available_area_.set_height(bottom_of_document);

  CalculateBackgroundParts();

  engine()->PageOffsetUpdated(available_area_.OffsetFromOrigin());
  engine()->PluginSizeUpdated(available_area_.size());

  if (document_size_.IsEmpty())
    return;
  paint_manager_.InvalidateRect(gfx::Rect(plugin_size_));
}

void PdfViewPluginBase::CalculateBackgroundParts() {
  background_parts_.clear();
  int left_width = available_area_.x();
  int right_start = available_area_.right();
  int right_width = std::abs(plugin_size().width() - available_area_.right());
  int bottom = std::min(available_area_.bottom(), plugin_size().height());

  // Note: we assume the display of the PDF document is always centered
  // horizontally, but not necessarily centered vertically.
  // Add the left rectangle.
  BackgroundPart part = {gfx::Rect(left_width, bottom), GetBackgroundColor()};
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);

  // Add the right rectangle.
  part.location = gfx::Rect(right_start, 0, right_width, bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);

  // Add the bottom rectangle.
  part.location = gfx::Rect(0, bottom, plugin_size().width(),
                            plugin_size().height() - bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
}

gfx::PointF PdfViewPluginBase::BoundScrollPositionToDocument(
    const gfx::PointF& scroll_position) {
  float max_x = std::max(
      document_size_.width() * float{zoom_} - plugin_dip_size_.width(), 0.0f);
  float x = base::ClampToRange(scroll_position.x(), 0.0f, max_x);
  float max_y = std::max(
      document_size_.height() * float{zoom_} - plugin_dip_size_.height(), 0.0f);
  float y = base::ClampToRange(scroll_position.y(), 0.0f, max_y);
  return gfx::PointF(x, y);
}

int PdfViewPluginBase::GetDocumentPixelWidth() const {
  return static_cast<int>(
      std::ceil(document_size_.width() * zoom() * device_scale()));
}

int PdfViewPluginBase::GetDocumentPixelHeight() const {
  return static_cast<int>(
      std::ceil(document_size_.height() * zoom() * device_scale()));
}

void PdfViewPluginBase::LoadAccessibility() {
  accessibility_state_ = AccessibilityState::kLoaded;
  AccessibilityDocInfo doc_info;
  doc_info.page_count = engine_->GetNumberOfPages();
  doc_info.text_accessible =
      engine_->HasPermission(PDFEngine::PERMISSION_COPY_ACCESSIBLE);
  doc_info.text_copyable = engine_->HasPermission(PDFEngine::PERMISSION_COPY);

  // A new document layout will trigger the creation of a new accessibility
  // tree, so |next_accessibility_page_index_| should be reset to ignore
  // outdated asynchronous calls of PrepareAndSetAccessibilityPageInfo().
  next_accessibility_page_index_ = 0;
  SetAccessibilityDocInfo(doc_info);

  // If the document contents isn't accessible, don't send anything more.
  if (!(engine_->HasPermission(PDFEngine::PERMISSION_COPY) ||
        engine_->HasPermission(PDFEngine::PERMISSION_COPY_ACCESSIBLE))) {
    return;
  }

  PrepareAndSetAccessibilityViewportInfo();

  // Schedule loading the first page.
  ScheduleTaskOnMainThread(
      kAccessibilityPageDelay,
      base::BindOnce(&PdfViewPluginBase::PrepareAndSetAccessibilityPageInfo,
                     GetWeakPtr()),
      0);
}

void PdfViewPluginBase::PrepareAndSetAccessibilityPageInfo(int32_t page_index) {
  // Outdated calls are ignored.
  if (page_index != next_accessibility_page_index_)
    return;
  ++next_accessibility_page_index_;

  AccessibilityPageInfo page_info;
  std::vector<AccessibilityTextRunInfo> text_runs;
  std::vector<AccessibilityCharInfo> chars;
  AccessibilityPageObjects page_objects;

  if (!GetAccessibilityInfo(engine(), page_index, page_info, text_runs, chars,
                            page_objects)) {
    return;
  }

  SetAccessibilityPageInfo(page_info, text_runs, chars, page_objects);

  // Schedule loading the next page.
  ScheduleTaskOnMainThread(
      kAccessibilityPageDelay,
      base::BindOnce(&PdfViewPluginBase::PrepareAndSetAccessibilityPageInfo,
                     GetWeakPtr()),
      page_index + 1);
}

void PdfViewPluginBase::PrepareAndSetAccessibilityViewportInfo() {
  AccessibilityViewportInfo viewport_info;
  viewport_info.scroll = gfx::ScaleToFlooredPoint(plugin_offset_, -1);
  viewport_info.offset = gfx::ScaleToFlooredPoint(available_area_.origin(),
                                                  1 / (device_scale_ * zoom_));
  viewport_info.zoom = zoom_;
  viewport_info.scale = device_scale_;
  viewport_info.focus_info = {FocusObjectType::kNone, 0, 0};

  engine_->GetSelection(&viewport_info.selection_start_page_index,
                        &viewport_info.selection_start_char_index,
                        &viewport_info.selection_end_page_index,
                        &viewport_info.selection_end_char_index);

  SetAccessibilityViewportInfo(viewport_info);
}

void PdfViewPluginBase::SetZoom(double scale) {
  double old_zoom = zoom_;
  zoom_ = scale;
  OnGeometryChanged(old_zoom, device_scale_);
}

void PdfViewPluginBase::HandleDisplayAnnotationsMessage(
    const base::Value& message) {
  engine()->DisplayAnnotations(message.FindBoolKey("display").value());
}

void PdfViewPluginBase::HandleGetNamedDestinationMessage(
    const base::Value& message) {
  const std::string* destination_name =
      message.FindStringKey("namedDestination");
  CHECK(destination_name);

  base::Optional<PDFEngine::NamedDestination> named_destination =
      engine()->GetNamedDestination(*destination_name);

  const int page_number = named_destination.has_value()
                              ? base::checked_cast<int>(named_destination->page)
                              : -1;

  base::Value reply = PrepareReplyMessage("getNamedDestinationReply", message);
  reply.SetIntKey("pageNumber", page_number);

  if (named_destination.has_value() && !named_destination->view.empty()) {
    std::ostringstream view_stream;
    view_stream << named_destination->view;
    if (named_destination->xyz_params.empty()) {
      for (unsigned long i = 0; i < named_destination->num_params; ++i)
        view_stream << "," << named_destination->params[i];
    } else {
      view_stream << "," << named_destination->xyz_params;
    }

    reply.SetStringKey("namedDestinationView", view_stream.str());
  }

  SendMessage(std::move(reply));
}

void PdfViewPluginBase::HandleGetSelectedTextMessage(
    const base::Value& message) {
  // Always return unix newlines to JavaScript.
  std::string selected_text;
  base::RemoveChars(engine()->GetSelectedText(), "\r", &selected_text);

  base::Value reply = PrepareReplyMessage("getSelectedTextReply", message);
  reply.SetStringKey("selectedText", selected_text);
  SendMessage(std::move(reply));
}

void PdfViewPluginBase::HandleRotateClockwiseMessage(
    const base::Value& /*message*/) {
  engine()->RotateClockwise();
}

void PdfViewPluginBase::HandleRotateCounterclockwiseMessage(
    const base::Value& /*message*/) {
  engine()->RotateCounterclockwise();
}

void PdfViewPluginBase::HandleSelectAllMessage(const base::Value& /*message*/) {
  engine()->SelectAll();
}

void PdfViewPluginBase::HandleSetBackgroundColorMessage(
    const base::Value& message) {
  const SkColor background_color =
      base::checked_cast<SkColor>(message.FindDoubleKey("color").value());
  SetBackgroundColor(background_color);
}

void PdfViewPluginBase::HandleSetReadOnlyMessage(const base::Value& message) {
  DCHECK(base::FeatureList::IsEnabled(features::kPdfViewerPresentationMode));
  engine()->SetReadOnly(message.FindBoolKey("enableReadOnly").value());
}

void PdfViewPluginBase::HandleSetTwoUpViewMessage(const base::Value& message) {
  engine()->SetTwoUpView(message.FindBoolKey("enableTwoUpView").value());
}

void PdfViewPluginBase::HandleViewportMessage(const base::Value& message) {
  const base::Value* layout_options_value =
      message.FindDictKey("layoutOptions");
  if (layout_options_value) {
    DocumentLayout::Options layout_options;
    layout_options.FromValue(*layout_options_value);
    // TODO(crbug.com/1013800): Eliminate need to get document size from here.
    document_size_ = engine()->ApplyDocumentLayout(layout_options);
    OnGeometryChanged(zoom_, device_scale_);
  }

  gfx::PointF scroll_position(message.FindDoubleKey("xOffset").value(),
                              message.FindDoubleKey("yOffset").value());
  double new_zoom = message.FindDoubleKey("zoom").value();
  const PinchPhase pinch_phase =
      static_cast<PinchPhase>(message.FindIntKey("pinchPhase").value());

  received_viewport_message_ = true;
  stop_scrolling_ = false;
  const double zoom_ratio = new_zoom / zoom_;

  if (pinch_phase == PinchPhase::kStart) {
    scroll_position_at_last_raster_ = scroll_position;
    last_bitmap_smaller_ = false;
    needs_reraster_ = false;
    return;
  }

  // When zooming in, we set a layer transform to avoid unneeded rerasters.
  // Also, if we're zooming out and the last time we rerastered was when
  // we were even further zoomed out (i.e. we pinch zoomed in and are now
  // pinch zooming back out in the same gesture), we update the layer
  // transform instead of rerastering.
  if (pinch_phase == PinchPhase::kUpdateZoomIn ||
      (pinch_phase == PinchPhase::kUpdateZoomOut && zoom_ratio > 1.0)) {
    // Get the coordinates of the center of the pinch gesture.
    const double pinch_x = message.FindDoubleKey("pinchX").value();
    const double pinch_y = message.FindDoubleKey("pinchY").value();
    gfx::Point pinch_center(pinch_x, pinch_y);

    // Get the pinch vector which represents the panning caused by the change in
    // pinch center between the start and the end of the gesture.
    const double pinch_vector_x = message.FindDoubleKey("pinchVectorX").value();
    const double pinch_vector_y = message.FindDoubleKey("pinchVectorY").value();
    gfx::Vector2d pinch_vector =
        gfx::Vector2d(pinch_vector_x * zoom_ratio, pinch_vector_y * zoom_ratio);

    gfx::Vector2d scroll_delta;
    // If the rendered document doesn't fill the display area we will
    // use `paint_offset` to anchor the paint vertically into the same place.
    // We use the scroll bars instead of the pinch vector to get the actual
    // position on screen of the paint.
    gfx::Vector2d paint_offset;

    if (plugin_size_.width() > GetDocumentPixelWidth() * zoom_ratio) {
      // We want to keep the paint in the middle but it must stay in the same
      // position relative to the scroll bars.
      paint_offset = gfx::Vector2d(0, (1 - zoom_ratio) * pinch_center.y());
      scroll_delta =
          gfx::Vector2d(0, (scroll_position.y() -
                            scroll_position_at_last_raster_.y() * zoom_ratio));

      pinch_vector = gfx::Vector2d();
      last_bitmap_smaller_ = true;
    } else if (last_bitmap_smaller_) {
      // When the document width covers the display area's width, we will anchor
      // the scroll bars disregarding where the actual pinch certer is.
      pinch_center = gfx::Point((plugin_size_.width() / device_scale_) / 2,
                                (plugin_size_.height() / device_scale_) / 2);
      const double zoom_when_doc_covers_plugin_width =
          zoom_ * plugin_size_.width() / GetDocumentPixelWidth();
      paint_offset = gfx::Vector2d(
          (1 - new_zoom / zoom_when_doc_covers_plugin_width) * pinch_center.x(),
          (1 - zoom_ratio) * pinch_center.y());
      pinch_vector = gfx::Vector2d();
      scroll_delta =
          gfx::Vector2d((scroll_position.x() -
                         scroll_position_at_last_raster_.x() * zoom_ratio),
                        (scroll_position.y() -
                         scroll_position_at_last_raster_.y() * zoom_ratio));
    }

    paint_manager().SetTransform(zoom_ratio, pinch_center,
                                 pinch_vector + paint_offset + scroll_delta,
                                 true);
    needs_reraster_ = false;
    return;
  }

  if (pinch_phase == PinchPhase::kUpdateZoomOut ||
      pinch_phase == PinchPhase::kEnd) {
    // We reraster on pinch zoom out in order to solve the invalid regions
    // that appear after zooming out.
    // On pinch end the scale is again 1.f and we request a reraster
    // in the new position.
    paint_manager().ClearTransform();
    last_bitmap_smaller_ = false;
    needs_reraster_ = true;

    // If we're rerastering due to zooming out, we need to update the scroll
    // position for the last raster, in case the user continues the gesture by
    // zooming in.
    scroll_position_at_last_raster_ = scroll_position;
  }

  // Bound the input parameters.
  new_zoom = std::max(kMinZoom, new_zoom);
  DCHECK(message.FindBoolKey("userInitiated").has_value());

  SetZoom(new_zoom);
  scroll_position = BoundScrollPositionToDocument(scroll_position);
  engine()->ScrolledToXPosition(scroll_position.x() * device_scale_);
  engine()->ScrolledToYPosition(scroll_position.y() * device_scale_);
}

void PdfViewPluginBase::DoPaint(const std::vector<gfx::Rect>& paint_rects,
                                std::vector<PaintReadyRect>& ready,
                                std::vector<gfx::Rect>& pending) {
  if (image_data_.drawsNothing()) {
    DCHECK(plugin_size_.IsEmpty());
    return;
  }

  PrepareForFirstPaint(ready);

  if (!received_viewport_message_ || !needs_reraster_)
    return;

  engine()->PrePaint();

  for (const gfx::Rect& paint_rect : paint_rects) {
    // Intersect with plugin area since there could be pending invalidates from
    // when the plugin area was larger.
    gfx::Rect rect = gfx::IntersectRects(paint_rect, gfx::Rect(plugin_size_));
    if (rect.IsEmpty())
      continue;

    // Paint the rendering of the PDF document.
    gfx::Rect pdf_rect = gfx::IntersectRects(rect, available_area_);
    if (!pdf_rect.IsEmpty()) {
      pdf_rect.Offset(-available_area_.x(), 0);

      std::vector<gfx::Rect> pdf_ready;
      std::vector<gfx::Rect> pdf_pending;
      engine()->Paint(pdf_rect, image_data_, pdf_ready, pdf_pending);
      for (gfx::Rect& ready_rect : pdf_ready) {
        ready_rect.Offset(available_area_.OffsetFromOrigin());
        ready.push_back(PaintReadyRect(ready_rect, GetPluginImageData()));
      }
      for (gfx::Rect& pending_rect : pdf_pending) {
        pending_rect.Offset(available_area_.OffsetFromOrigin());
        pending.push_back(pending_rect);
      }
    }

    // Ensure the region above the first page (if any) is filled;
    const int32_t first_page_ypos = 0 == engine()->GetNumberOfPages()
                                        ? 0
                                        : engine()->GetPageScreenRect(0).y();
    if (rect.y() < first_page_ypos) {
      gfx::Rect region = gfx::IntersectRects(
          rect, gfx::Rect(gfx::Size(plugin_size_.width(), first_page_ypos)));
      ready.push_back(PaintReadyRect(region, GetPluginImageData()));
      image_data_.erase(background_color_, gfx::RectToSkIRect(region));
    }

    // Ensure the background parts are filled.
    for (const BackgroundPart& background_part : background_parts_) {
      gfx::Rect intersection =
          gfx::IntersectRects(background_part.location, rect);
      if (!intersection.IsEmpty()) {
        image_data_.erase(background_part.color,
                          gfx::RectToSkIRect(intersection));
        ready.push_back(PaintReadyRect(intersection, GetPluginImageData()));
      }
    }
  }

  engine()->PostPaint();

  InvalidateAfterPaintDone();
}

void PdfViewPluginBase::PrepareForFirstPaint(
    std::vector<PaintReadyRect>& ready) {
  if (!first_paint_)
    return;

  // Fill the image data buffer with the background color.
  first_paint_ = false;
  image_data_.eraseColor(background_color_);
  gfx::Rect rect(gfx::SkISizeToSize(image_data_.dimensions()));
  ready.push_back(
      PaintReadyRect(rect, GetPluginImageData(), /*flush_now=*/true));
}

void PdfViewPluginBase::ClearDeferredInvalidates(
    int32_t /*unused_but_required*/) {
  DCHECK(!in_paint_);
  for (const gfx::Rect& rect : deferred_invalidates_)
    Invalidate(rect);
  deferred_invalidates_.clear();
}

}  // namespace chrome_pdf
