// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/accessibility.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/content_restriction.h"
#include "pdf/document_layout.h"
#include "pdf/document_metadata.h"
#include "pdf/paint_ready_rect.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "pdf/ui/file_name.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "url/gurl.h"

namespace chrome_pdf {

namespace {

// A delay to wait between each accessibility page to keep the system
// responsive.
constexpr base::TimeDelta kAccessibilityPageDelay = base::Milliseconds(100);

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

void PdfViewPluginBase::DidScroll(const gfx::Vector2d& offset) {
  if (!image_data_.drawsNothing())
    paint_manager_.ScrollRect(available_area_, offset);
}

void PdfViewPluginBase::ScrollToX(int x_screen_coords) {
  const float x_scroll_pos = x_screen_coords / device_scale();

  base::Value::Dict message;
  message.Set("type", "setScrollPosition");
  message.Set("x", static_cast<double>(x_scroll_pos));
  SendMessage(std::move(message));
}

void PdfViewPluginBase::ScrollToY(int y_screen_coords) {
  const float y_scroll_pos = y_screen_coords / device_scale();

  base::Value::Dict message;
  message.Set("type", "setScrollPosition");
  message.Set("y", static_cast<double>(y_scroll_pos));
  SendMessage(std::move(message));
}

void PdfViewPluginBase::ScrollBy(const gfx::Vector2d& delta) {
  const float x_delta = delta.x() / device_scale();
  const float y_delta = delta.y() / device_scale();

  base::Value::Dict message;
  message.Set("type", "scrollBy");
  message.Set("x", static_cast<double>(x_delta));
  message.Set("y", static_cast<double>(y_delta));
  SendMessage(std::move(message));
}

void PdfViewPluginBase::ScrollToPage(int page) {
  if (!engine() || engine()->GetNumberOfPages() == 0)
    return;

  base::Value::Dict message;
  message.Set("type", "goToPage");
  message.Set("page", page);
  SendMessage(std::move(message));
}

void PdfViewPluginBase::NavigateTo(const std::string& url,
                                   WindowOpenDisposition disposition) {
  base::Value::Dict message;
  message.Set("type", "navigate");
  message.Set("url", url);
  message.Set("disposition", static_cast<int>(disposition));
  SendMessage(std::move(message));
}

void PdfViewPluginBase::NavigateToDestination(int page,
                                              const float* x,
                                              const float* y,
                                              const float* zoom) {
  base::Value::Dict message;
  message.Set("type", "navigateToDestination");
  message.Set("page", page);
  if (x)
    message.Set("x", static_cast<double>(*x));
  if (y)
    message.Set("y", static_cast<double>(*y));
  if (zoom)
    message.Set("zoom", static_cast<double>(*zoom));
  SendMessage(std::move(message));
}

void PdfViewPluginBase::NotifyTouchSelectionOccurred() {
  base::Value::Dict message;
  message.Set("type", "touchSelectionOccurred");
  SendMessage(std::move(message));
}

void PdfViewPluginBase::Email(const std::string& to,
                              const std::string& cc,
                              const std::string& bcc,
                              const std::string& subject,
                              const std::string& body) {
  base::Value::Dict message;
  message.Set("type", "email");
  message.Set("to", base::EscapeUrlEncodedData(to, false));
  message.Set("cc", base::EscapeUrlEncodedData(cc, false));
  message.Set("bcc", base::EscapeUrlEncodedData(bcc, false));
  message.Set("subject", base::EscapeUrlEncodedData(subject, false));
  message.Set("body", base::EscapeUrlEncodedData(body, false));
  SendMessage(std::move(message));
}

void PdfViewPluginBase::DocumentLoadComplete() {
  DCHECK_EQ(DocumentLoadState::kLoading, document_load_state_);
  document_load_state_ = DocumentLoadState::kComplete;

  UserMetricsRecordAction("PDF.LoadSuccess");

  // Clear the focus state for on-screen keyboards.
  FormFieldFocusChange(PDFEngine::FocusFieldType::kNoFocus);

  if (IsPrintPreview())
    OnPrintPreviewLoaded();

  OnDocumentLoadComplete();

  if (!full_frame())
    return;

  DidStopLoading();
  SetContentRestrictions(GetContentRestrictions());
}

void PdfViewPluginBase::DocumentLoadFailed() {
  DCHECK_EQ(DocumentLoadState::kLoading, document_load_state_);
  document_load_state_ = DocumentLoadState::kFailed;

  UserMetricsRecordAction("PDF.LoadFailure");

  // Send a progress value of -1 to indicate a failure.
  SendLoadingProgress(-1);

  DidStopLoading();

  paint_manager_.InvalidateRect(gfx::Rect(plugin_rect().size()));
}

void PdfViewPluginBase::DocumentLoadProgress(uint32_t available,
                                             uint32_t doc_size) {
  double progress = 0.0;
  if (doc_size > 0) {
    progress = 100.0 * static_cast<double>(available) / doc_size;
  } else {
    // Use heuristics when the document size is unknown.
    // Progress logarithmically from 0 to 100M.
    static const double kFactor = std::log(100'000'000.0) / 100.0;
    if (available > 0)
      progress =
          std::min(std::log(static_cast<double>(available)) / kFactor, 100.0);
  }

  // DocumentLoadComplete() will send the 100% load progress.
  if (progress >= 100)
    return;

  // Avoid sending too many progress messages over PostMessage.
  if (progress <= last_progress_sent_ + 1)
    return;

  SendLoadingProgress(progress);
}

void PdfViewPluginBase::FormFieldFocusChange(PDFEngine::FocusFieldType type) {
  base::Value::Dict message;
  message.Set("type", "formFocusChange");
  message.Set("focused", type != PDFEngine::FocusFieldType::kNoFocus);
  SendMessage(std::move(message));

  SetFormTextFieldInFocus(type == PDFEngine::FocusFieldType::kText);
}

void PdfViewPluginBase::SetIsSelecting(bool is_selecting) {
  base::Value::Dict message;
  message.Set("type", "setIsSelecting");
  message.Set("isSelecting", is_selecting);
  SendMessage(std::move(message));
}

void PdfViewPluginBase::SelectionChanged(const gfx::Rect& left,
                                         const gfx::Rect& right) {
  gfx::PointF left_point(left.x() + available_area_.x(), left.y());
  gfx::PointF right_point(right.x() + available_area_.x(), right.y());

  const float inverse_scale = 1.0f / device_scale();
  left_point.Scale(inverse_scale);
  right_point.Scale(inverse_scale);

  NotifySelectionChanged(left_point, left.height() * inverse_scale, right_point,
                         right.height() * inverse_scale);

  if (accessibility_state_ == AccessibilityState::kLoaded)
    PrepareAndSetAccessibilityViewportInfo();
}

void PdfViewPluginBase::DocumentFocusChanged(bool document_has_focus) {
  base::Value::Dict message;
  message.Set("type", "documentFocusChanged");
  message.Set("hasFocus", document_has_focus);
  SendMessage(std::move(message));
}

void PdfViewPluginBase::SendLoadingProgress(double percentage) {
  DCHECK(percentage == -1 || (percentage >= 0 && percentage <= 100));
  last_progress_sent_ = percentage;

  base::Value::Dict message;
  message.Set("type", "loadProgress");
  message.Set("progress", percentage);
  SendMessage(std::move(message));
}

void PdfViewPluginBase::OnPaint(const std::vector<gfx::Rect>& paint_rects,
                                std::vector<PaintReadyRect>& ready,
                                std::vector<gfx::Rect>& pending) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  DoPaint(paint_rects, ready, pending);
}

int PdfViewPluginBase::GetContentRestrictions() const {
  int content_restrictions = kContentRestrictionCut | kContentRestrictionPaste;
  if (!engine()->HasPermission(DocumentPermission::kCopy))
    content_restrictions |= kContentRestrictionCopy;

  if (!engine()->HasPermission(DocumentPermission::kPrintLowQuality) &&
      !engine()->HasPermission(DocumentPermission::kPrintHighQuality)) {
    content_restrictions |= kContentRestrictionPrint;
  }

  return content_restrictions;
}

AccessibilityDocInfo PdfViewPluginBase::GetAccessibilityDocInfo() const {
  AccessibilityDocInfo doc_info;
  doc_info.page_count = engine()->GetNumberOfPages();
  doc_info.text_accessible =
      engine()->HasPermission(DocumentPermission::kCopyAccessible);
  doc_info.text_copyable = engine()->HasPermission(DocumentPermission::kCopy);
  return doc_info;
}

void PdfViewPluginBase::InvalidateAfterPaintDone() {
  if (deferred_invalidates_.empty())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PdfViewPluginBase::ClearDeferredInvalidates,
                                GetWeakPtr()));
}

void PdfViewPluginBase::OnGeometryChanged(double old_zoom,
                                          float old_device_scale) {
  RecalculateAreas(old_zoom, old_device_scale);

  if (accessibility_state_ == AccessibilityState::kLoaded)
    PrepareAndSetAccessibilityViewportInfo();
}

void PdfViewPluginBase::RecalculateAreas(double old_zoom,
                                         float old_device_scale) {
  if (zoom_ != old_zoom || device_scale() != old_device_scale)
    engine()->ZoomUpdated(zoom_ * device_scale());

  available_area_ = gfx::Rect(plugin_rect().size());
  int doc_width = GetDocumentPixelWidth();
  if (doc_width < available_area_.width()) {
    // Center the document horizontally inside the plugin rectangle.
    available_area_.Offset((plugin_rect().width() - doc_width) / 2, 0);
    available_area_.set_width(doc_width);
  }

  // The distance between top of the plugin and the bottom of the document in
  // pixels.
  int bottom_of_document = GetDocumentPixelHeight();
  if (bottom_of_document < plugin_rect().height())
    available_area_.set_height(bottom_of_document);

  CalculateBackgroundParts();

  engine()->PageOffsetUpdated(available_area_.OffsetFromOrigin());
  engine()->PluginSizeUpdated(available_area_.size());
}

void PdfViewPluginBase::CalculateBackgroundParts() {
  background_parts_.clear();
  int left_width = available_area_.x();
  int right_start = available_area_.right();
  int right_width = std::abs(plugin_rect().width() - available_area_.right());
  int bottom = std::min(available_area_.bottom(), plugin_rect().height());

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
  part.location = gfx::Rect(0, bottom, plugin_rect().width(),
                            plugin_rect().height() - bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
}

gfx::PointF PdfViewPluginBase::GetScrollPositionFromOffset(
    const gfx::Vector2dF& scroll_offset) const {
  gfx::PointF scroll_origin;

  // TODO(crbug.com/1140374): Right-to-left scrolling currently is not
  // compatible with the PDF viewer's sticky "scroller" element.
  if (ui_direction() == base::i18n::RIGHT_TO_LEFT && IsPrintPreview()) {
    scroll_origin.set_x(
        std::max(document_size_.width() * static_cast<float>(zoom_) -
                     plugin_dip_size().width(),
                 0.0f));
  }

  return scroll_origin + scroll_offset;
}

int PdfViewPluginBase::GetDocumentPixelWidth() const {
  return static_cast<int>(
      std::ceil(document_size_.width() * zoom() * device_scale()));
}

int PdfViewPluginBase::GetDocumentPixelHeight() const {
  return static_cast<int>(
      std::ceil(document_size_.height() * zoom() * device_scale()));
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

  SetAccessibilityPageInfo(std::move(page_info), std::move(text_runs),
                           std::move(chars), std::move(page_objects));

  // Schedule loading the next page.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PdfViewPluginBase::PrepareAndSetAccessibilityPageInfo,
                     GetWeakPtr(), page_index + 1),
      kAccessibilityPageDelay);
}

void PdfViewPluginBase::PrepareAndSetAccessibilityViewportInfo() {
  AccessibilityViewportInfo viewport_info;
  viewport_info.offset = gfx::ScaleToFlooredPoint(available_area_.origin(),
                                                  1 / (device_scale() * zoom_));
  viewport_info.zoom = zoom_;
  viewport_info.scale = device_scale();
  viewport_info.focus_info = {FocusObjectType::kNone, 0, 0};

  engine()->GetSelection(&viewport_info.selection_start_page_index,
                         &viewport_info.selection_start_char_index,
                         &viewport_info.selection_end_page_index,
                         &viewport_info.selection_end_char_index);

  SetAccessibilityViewportInfo(std::move(viewport_info));
}

void PdfViewPluginBase::SetZoom(double scale) {
  double old_zoom = zoom_;
  zoom_ = scale;

  OnGeometryChanged(old_zoom, device_scale());
  if (!document_size_.IsEmpty())
    paint_manager_.InvalidateRect(gfx::Rect(plugin_rect().size()));
}

void PdfViewPluginBase::DoPaint(const std::vector<gfx::Rect>& paint_rects,
                                std::vector<PaintReadyRect>& ready,
                                std::vector<gfx::Rect>& pending) {
  if (image_data_.drawsNothing()) {
    DCHECK(plugin_rect().IsEmpty());
    return;
  }

  PrepareForFirstPaint(ready);

  if (!received_viewport_message() || !needs_reraster())
    return;

  engine()->PrePaint();

  std::vector<gfx::Rect> ready_rects;
  for (const gfx::Rect& paint_rect : paint_rects) {
    // Intersect with plugin area since there could be pending invalidates from
    // when the plugin area was larger.
    gfx::Rect rect =
        gfx::IntersectRects(paint_rect, gfx::Rect(plugin_rect().size()));
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
        ready_rects.push_back(ready_rect);
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
          rect, gfx::Rect(gfx::Size(plugin_rect().width(), first_page_ypos)));
      image_data_.erase(GetBackgroundColor(), gfx::RectToSkIRect(region));
      ready_rects.push_back(region);
    }

    // Ensure the background parts are filled.
    for (const BackgroundPart& background_part : background_parts_) {
      gfx::Rect intersection =
          gfx::IntersectRects(background_part.location, rect);
      if (!intersection.IsEmpty()) {
        image_data_.erase(background_part.color,
                          gfx::RectToSkIRect(intersection));
        ready_rects.push_back(intersection);
      }
    }
  }

  engine()->PostPaint();

  // TODO(crbug.com/1263614): Write pixels directly to the `SkSurface` in
  // `PaintManager`, rather than using an intermediate `SkBitmap` and `SkImage`.
  sk_sp<SkImage> painted_image = image_data_.asImage();
  for (const gfx::Rect& ready_rect : ready_rects)
    ready.emplace_back(ready_rect, painted_image);

  InvalidateAfterPaintDone();
}

void PdfViewPluginBase::ClearDeferredInvalidates() {
  DCHECK(!in_paint_);
  for (const gfx::Rect& rect : deferred_invalidates_)
    Invalidate(rect);
  deferred_invalidates_.clear();
}

void PdfViewPluginBase::LoadAccessibility() {
  accessibility_state_ = AccessibilityState::kLoaded;

  // A new document layout will trigger the creation of a new accessibility
  // tree, so `next_accessibility_page_index_` should be reset to ignore
  // outdated asynchronous calls of PrepareAndSetAccessibilityPageInfo().
  next_accessibility_page_index_ = 0;
  SetAccessibilityDocInfo(GetAccessibilityDocInfo());

  // If the document contents isn't accessible, don't send anything more.
  if (!(engine()->HasPermission(DocumentPermission::kCopy) ||
        engine()->HasPermission(DocumentPermission::kCopyAccessible))) {
    return;
  }

  PrepareAndSetAccessibilityViewportInfo();

  // Schedule loading the first page.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PdfViewPluginBase::PrepareAndSetAccessibilityPageInfo,
                     GetWeakPtr(), /*page_index=*/0),
      kAccessibilityPageDelay);
}

gfx::Point PdfViewPluginBase::FrameToPdfCoordinates(
    const gfx::PointF& frame_coordinates) const {
  // TODO(crbug.com/1288847): Use methods on `blink::WebPluginContainer`.
  return gfx::ToFlooredPoint(
             gfx::ScalePoint(frame_coordinates, device_scale())) -
         gfx::Vector2d(available_area_.x(), 0);
}

}  // namespace chrome_pdf
