// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/values.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/ppapi_migration/image.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

// static
constexpr double PdfViewPluginBase::kMinZoom;

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
          {"rotateClockwise", &PdfViewPluginBase::HandleRotateClockwiseMessage},
          {"rotateCounterclockwise",
           &PdfViewPluginBase::HandleRotateCounterclockwiseMessage},
          {"selectAll", &PdfViewPluginBase::HandleSelectAllMessage},
          {"setBackgroundColor",
           &PdfViewPluginBase::HandleSetBackgroundColorMessage},
          {"setReadOnly", &PdfViewPluginBase::HandleSetReadOnlyMessage},
          {"setTwoUpView", &PdfViewPluginBase::HandleSetTwoUpViewMessage},
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
                                std::vector<PaintReadyRect>* ready,
                                std::vector<gfx::Rect>* pending) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  DoPaint(paint_rects, ready, pending);
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
  int bottom_of_document =
      GetDocumentPixelHeight() +
      (top_toolbar_height_in_viewport_coords() * device_scale());
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

int PdfViewPluginBase::GetDocumentPixelWidth() const {
  return static_cast<int>(
      std::ceil(document_size_.width() * zoom() * device_scale()));
}

int PdfViewPluginBase::GetDocumentPixelHeight() const {
  return static_cast<int>(
      std::ceil(document_size_.height() * zoom() * device_scale()));
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

void PdfViewPluginBase::ClearDeferredInvalidates(
    int32_t /*unused_but_required*/) {
  DCHECK(!in_paint_);
  for (const gfx::Rect& rect : deferred_invalidates_)
    Invalidate(rect);
  deferred_invalidates_.clear();
}

}  // namespace chrome_pdf
