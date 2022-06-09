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
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
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
#include "pdf/ppapi_migration/result_codes.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "pdf/ui/file_name.h"
#include "pdf/ui/thumbnail.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
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

// The minimum zoom level allowed.
constexpr double kMinZoom = 0.01;

// A delay to wait between each accessibility page to keep the system
// responsive.
constexpr base::TimeDelta kAccessibilityPageDelay = base::Milliseconds(100);

// Same value as printing::COMPLETE_PREVIEW_DOCUMENT_INDEX.
constexpr int kCompletePDFIndex = -1;
// A different negative value to differentiate itself from `kCompletePDFIndex`.
constexpr int kInvalidPDFIndex = -2;

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
base::Value::Dict PrepareReplyMessage(base::StringPiece reply_type,
                                      const base::Value::Dict& message) {
  DCHECK_EQ(reply_type, *message.FindString("type") + "Reply");

  base::Value::Dict reply;
  reply.Set("type", reply_type);
  reply.Set("messageId", *message.FindString("messageId"));
  return reply;
}

bool IsPrintPreviewUrl(base::StringPiece url) {
  return base::StartsWith(url, PdfViewPluginBase::kChromeUntrustedPrintHost);
}

int ExtractPrintPreviewPageIndex(base::StringPiece src_url) {
  // Sample `src_url` format: chrome-untrusted://print/id/page_index/print.pdf
  // The page_index is zero-based, but can be negative with special meanings.
  std::vector<base::StringPiece> url_substr = base::SplitStringPiece(
      src_url.substr(PdfViewPluginBase::kChromeUntrustedPrintHost.size()), "/",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 3)
    return kInvalidPDFIndex;

  if (url_substr[2] != "print.pdf")
    return kInvalidPDFIndex;

  int page_index = 0;
  if (!base::StringToInt(url_substr[1], &page_index))
    return kInvalidPDFIndex;
  return page_index;
}

bool IsPreviewingPDF(int print_preview_page_count) {
  return print_preview_page_count == 0;
}

}  // namespace

// static
constexpr base::StringPiece PdfViewPluginBase::kChromePrintHost;

// static
constexpr base::StringPiece PdfViewPluginBase::kChromeUntrustedPrintHost;

PdfViewPluginBase::PdfViewPluginBase() = default;

PdfViewPluginBase::~PdfViewPluginBase() = default;

void PdfViewPluginBase::ProposeDocumentLayout(const DocumentLayout& layout) {
  base::Value::Dict message;
  message.Set("type", "documentDimensions");
  message.Set("width", layout.size().width());
  message.Set("height", layout.size().height());
  message.Set("layoutOptions", layout.options().ToValue());
  base::Value::List page_dimensions;
  for (size_t i = 0; i < layout.page_count(); ++i)
    page_dimensions.Append(DictFromRect(layout.page_rect(i)));
  message.Set("pageDimensions", std::move(page_dimensions));
  SendMessage(std::move(message));

  // Reload the accessibility tree on layout changes because the relative page
  // bounds are no longer valid.
  if (layout.dirty() && accessibility_state_ == AccessibilityState::kLoaded)
    LoadAccessibility();
}

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
  const float x_scroll_pos = x_screen_coords / device_scale_;

  base::Value::Dict message;
  message.Set("type", "setScrollPosition");
  message.Set("x", static_cast<double>(x_scroll_pos));
  SendMessage(std::move(message));
}

void PdfViewPluginBase::ScrollToY(int y_screen_coords) {
  const float y_scroll_pos = y_screen_coords / device_scale_;

  base::Value::Dict message;
  message.Set("type", "setScrollPosition");
  message.Set("y", static_cast<double>(y_scroll_pos));
  SendMessage(std::move(message));
}

void PdfViewPluginBase::ScrollBy(const gfx::Vector2d& delta) {
  const float x_delta = delta.x() / device_scale_;
  const float y_delta = delta.y() / device_scale_;

  base::Value::Dict message;
  message.Set("type", "scrollBy");
  message.Set("x", static_cast<double>(x_delta));
  message.Set("y", static_cast<double>(y_delta));
  SendMessage(std::move(message));
}

void PdfViewPluginBase::ScrollToPage(int page) {
  if (!engine_ || engine_->GetNumberOfPages() == 0)
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

void PdfViewPluginBase::GetDocumentPassword(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK(password_callback_.is_null());
  password_callback_ = std::move(callback);

  base::Value::Dict message;
  message.Set("type", "getPassword");
  SendMessage(std::move(message));
}

void PdfViewPluginBase::Beep() {
  base::Value::Dict message;
  message.Set("type", "beep");
  SendMessage(std::move(message));
}

std::string PdfViewPluginBase::GetURL() {
  return url_;
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

void PdfViewPluginBase::Print() {
  if (!engine_)
    return;

  const bool can_print =
      engine_->HasPermission(DocumentPermission::kPrintLowQuality) ||
      engine_->HasPermission(DocumentPermission::kPrintHighQuality);
  if (!can_print)
    return;

  InvokePrintDialog();
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

  if (accessibility_state_ == AccessibilityState::kPending)
    LoadAccessibility();

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

  paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));
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

  const float inverse_scale = 1.0f / device_scale_;
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

void PdfViewPluginBase::SetLinkUnderCursor(
    const std::string& link_under_cursor) {
  link_under_cursor_ = link_under_cursor;
}

bool PdfViewPluginBase::HandleInputEvent(const blink::WebInputEvent& event) {
  // Ignore user input in read-only mode.
  if (engine()->IsReadOnly())
    return false;

  // `engine()` expects input events in device coordinates.
  std::unique_ptr<blink::WebInputEvent> transformed_event =
      ui::TranslateAndScaleWebInputEvent(
          event, gfx::Vector2dF(-available_area_.x() / device_scale_, 0),
          device_scale_);

  const blink::WebInputEvent& event_to_handle =
      transformed_event ? *transformed_event : event;

  if (engine()->HandleInputEvent(event_to_handle))
    return true;

  // Middle click is used for scrolling and is handled by the container page.
  if (blink::WebInputEvent::IsMouseEventType(event_to_handle.GetType()) &&
      static_cast<const blink::WebMouseEvent&>(event_to_handle).button ==
          blink::WebPointerProperties::Button::kMiddle) {
    return false;
  }

  // Return true for unhandled clicks so the plugin takes focus.
  return event_to_handle.GetType() == blink::WebInputEvent::Type::kMouseDown;
}

void PdfViewPluginBase::HandleMessage(const base::Value::Dict& message) {
  using MessageHandler = void (PdfViewPluginBase::*)(const base::Value::Dict&);
  static constexpr auto kMessageHandlers =
      base::MakeFixedFlatMap<base::StringPiece, MessageHandler>({
          {"displayAnnotations",
           &PdfViewPluginBase::HandleDisplayAnnotationsMessage},
          {"getNamedDestination",
           &PdfViewPluginBase::HandleGetNamedDestinationMessage},
          {"getPasswordComplete",
           &PdfViewPluginBase::HandleGetPasswordCompleteMessage},
          {"getSelectedText", &PdfViewPluginBase::HandleGetSelectedTextMessage},
          {"getThumbnail", &PdfViewPluginBase::HandleGetThumbnailMessage},
          {"print", &PdfViewPluginBase::HandlePrintMessage},
          {"loadPreviewPage", &PdfViewPluginBase::HandleLoadPreviewPageMessage},
          {"resetPrintPreviewMode",
           &PdfViewPluginBase::HandleResetPrintPreviewModeMessage},
          {"rotateClockwise", &PdfViewPluginBase::HandleRotateClockwiseMessage},
          {"rotateCounterclockwise",
           &PdfViewPluginBase::HandleRotateCounterclockwiseMessage},
          {"saveAttachment", &PdfViewPluginBase::HandleSaveAttachmentMessage},
          {"selectAll", &PdfViewPluginBase::HandleSelectAllMessage},
          {"setPresentationMode",
           &PdfViewPluginBase::HandleSetPresentationModeMessage},
          {"setTwoUpView", &PdfViewPluginBase::HandleSetTwoUpViewMessage},
          {"stopScrolling", &PdfViewPluginBase::HandleStopScrollingMessage},
          {"viewport", &PdfViewPluginBase::HandleViewportMessage},
      });

  MessageHandler handler = kMessageHandlers.at(*message.FindString("type"));
  (this->*handler)(message);
}

void PdfViewPluginBase::SendLoadingProgress(double percentage) {
  DCHECK(percentage == -1 || (percentage >= 0 && percentage <= 100));
  last_progress_sent_ = percentage;

  base::Value::Dict message;
  message.Set("type", "loadProgress");
  message.Set("progress", percentage);
  SendMessage(std::move(message));
}

void PdfViewPluginBase::SendPrintPreviewLoadedNotification() {
  base::Value::Dict message;
  message.Set("type", "printPreviewLoaded");
  SendMessage(std::move(message));
}

void PdfViewPluginBase::OnPaint(const std::vector<gfx::Rect>& paint_rects,
                                std::vector<PaintReadyRect>& ready,
                                std::vector<gfx::Rect>& pending) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  DoPaint(paint_rects, ready, pending);
}

void PdfViewPluginBase::PreviewDocumentLoadComplete() {
  if (preview_document_load_state_ != DocumentLoadState::kLoading ||
      preview_pages_info_.empty()) {
    return;
  }

  preview_document_load_state_ = DocumentLoadState::kComplete;

  int dest_page_index = preview_pages_info_.front().second;
  DCHECK_GT(dest_page_index, 0);
  preview_pages_info_.pop();
  DCHECK(preview_engine_);
  engine()->AppendPage(preview_engine_.get(), dest_page_index);

  ++print_preview_loaded_page_count_;
  LoadNextPreviewPage();
}

void PdfViewPluginBase::PreviewDocumentLoadFailed() {
  UserMetricsRecordAction("PDF.PreviewDocumentLoadFailure");
  if (preview_document_load_state_ != DocumentLoadState::kLoading ||
      preview_pages_info_.empty()) {
    return;
  }

  // Even if a print preview page failed to load, keep going.
  preview_document_load_state_ = DocumentLoadState::kFailed;
  preview_pages_info_.pop();
  ++print_preview_loaded_page_count_;
  LoadNextPreviewPage();
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

void PdfViewPluginBase::InitializeEngineForTesting(
    std::unique_ptr<PDFiumEngine> engine) {
  DCHECK(engine);
  engine_ = std::move(engine);
}

void PdfViewPluginBase::DestroyEngine() {
  engine_.reset();
}

void PdfViewPluginBase::DestroyPreviewEngine() {
  preview_engine_.reset();
}

void PdfViewPluginBase::set_engine(std::unique_ptr<PDFiumEngine> engine) {
  engine_ = std::move(engine);
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

blink::WebPrintPresetOptions PdfViewPluginBase::GetPrintPresetOptions() {
  blink::WebPrintPresetOptions options;
  options.is_scaling_disabled = !engine_->GetPrintScaling();
  options.copies = engine_->GetCopiesToPrint();
  options.duplex_mode = engine_->GetDuplexMode();
  options.uniform_page_size = engine_->GetUniformPageSizePoints();
  return options;
}

int PdfViewPluginBase::PrintBegin(const blink::WebPrintParams& print_params) {
  // The returned value is always equal to the number of pages in the PDF
  // document irrespective of the printable area.
  int32_t ret = engine()->GetNumberOfPages();
  if (!ret)
    return 0;

  if (!engine()->HasPermission(DocumentPermission::kPrintLowQuality))
    return 0;

  print_params_ = print_params;
  if (!engine()->HasPermission(DocumentPermission::kPrintHighQuality))
    print_params_->rasterize_pdf = true;

  engine()->PrintBegin();
  return ret;
}

std::vector<uint8_t> PdfViewPluginBase::PrintPages(
    const std::vector<int>& page_numbers) {
  print_pages_called_ = true;
  return engine()->PrintPages(page_numbers, print_params_.value());
}

void PdfViewPluginBase::PrintEnd() {
  if (print_pages_called_)
    UserMetricsRecordAction("PDF.PrintPage");
  print_pages_called_ = false;
  print_params_.reset();
  engine_->PrintEnd();
}

void PdfViewPluginBase::UpdateGeometryOnPluginRectChanged(
    const gfx::Rect& new_plugin_rect,
    float new_device_scale) {
  DCHECK_GT(new_device_scale, 0.0f);

  if (new_device_scale == device_scale_ && new_plugin_rect == plugin_rect_)
    return;

  const float old_device_scale = device_scale_;
  device_scale_ = new_device_scale;
  plugin_rect_ = new_plugin_rect;
  // TODO(crbug.com/1250173): For the Pepper-free plugin, `plugin_dip_size_` is
  // calculated from the `window_rect` in PdfViewWebPlugin::UpdateGeometry().
  // We should try to avoid the downscaling during this calculation process and
  // maybe migrate off `plugin_dip_size_`.
  plugin_dip_size_ =
      gfx::ScaleToEnclosingRect(new_plugin_rect, 1.0f / new_device_scale)
          .size();

  paint_manager_.SetSize(plugin_rect_.size(), device_scale_);

  // Initialize the image data buffer if the context size changes.
  const gfx::Size old_image_size = gfx::SkISizeToSize(image_data_.dimensions());
  const gfx::Size new_image_size =
      PaintManager::GetNewContextSize(old_image_size, plugin_rect_.size());
  if (new_image_size != old_image_size) {
    image_data_.allocPixels(
        SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(new_image_size)));
    first_paint_ = true;
  }

  // Skip updating the geometry if the new image data buffer is empty.
  if (image_data_.drawsNothing())
    return;

  OnGeometryChanged(zoom_, old_device_scale);
}

void PdfViewPluginBase::RecalculateAreas(double old_zoom,
                                         float old_device_scale) {
  if (zoom_ != old_zoom || device_scale_ != old_device_scale)
    engine()->ZoomUpdated(zoom_ * device_scale_);

  available_area_ = gfx::Rect(plugin_rect_.size());
  int doc_width = GetDocumentPixelWidth();
  if (doc_width < available_area_.width()) {
    // Center the document horizontally inside the plugin rectangle.
    available_area_.Offset((plugin_rect_.width() - doc_width) / 2, 0);
    available_area_.set_width(doc_width);
  }

  // The distance between top of the plugin and the bottom of the document in
  // pixels.
  int bottom_of_document = GetDocumentPixelHeight();
  if (bottom_of_document < plugin_rect_.height())
    available_area_.set_height(bottom_of_document);

  CalculateBackgroundParts();

  engine()->PageOffsetUpdated(available_area_.OffsetFromOrigin());
  engine()->PluginSizeUpdated(available_area_.size());
}

void PdfViewPluginBase::CalculateBackgroundParts() {
  background_parts_.clear();
  int left_width = available_area_.x();
  int right_start = available_area_.right();
  int right_width = std::abs(plugin_rect_.width() - available_area_.right());
  int bottom = std::min(available_area_.bottom(), plugin_rect_.height());

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
  part.location = gfx::Rect(0, bottom, plugin_rect_.width(),
                            plugin_rect_.height() - bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
}

gfx::PointF PdfViewPluginBase::GetScrollPositionFromOffset(
    const gfx::Vector2dF& scroll_offset) const {
  gfx::PointF scroll_origin;

  // TODO(crbug.com/1140374): Right-to-left scrolling currently is not
  // compatible with the PDF viewer's sticky "scroller" element.
  if (ui_direction_ == base::i18n::RIGHT_TO_LEFT && IsPrintPreview()) {
    scroll_origin.set_x(
        std::max(document_size_.width() * static_cast<float>(zoom_) -
                     plugin_dip_size_.width(),
                 0.0f));
  }

  return scroll_origin + scroll_offset;
}

void PdfViewPluginBase::UpdateScroll(const gfx::PointF& scroll_position) {
  if (stop_scrolling_)
    return;

  float max_x = std::max(document_size_.width() * static_cast<float>(zoom_) -
                             plugin_dip_size_.width(),
                         0.0f);
  float max_y = std::max(document_size_.height() * static_cast<float>(zoom_) -
                             plugin_dip_size_.height(),
                         0.0f);

  gfx::PointF scaled_scroll_position(
      base::clamp(scroll_position.x(), 0.0f, max_x),
      base::clamp(scroll_position.y(), 0.0f, max_y));
  scaled_scroll_position.Scale(device_scale_);

  engine()->ScrolledToXPosition(scaled_scroll_position.x());
  engine()->ScrolledToYPosition(scaled_scroll_position.y());
}

int PdfViewPluginBase::GetDocumentPixelWidth() const {
  return static_cast<int>(
      std::ceil(document_size_.width() * zoom() * device_scale()));
}

int PdfViewPluginBase::GetDocumentPixelHeight() const {
  return static_cast<int>(
      std::ceil(document_size_.height() * zoom() * device_scale()));
}

void PdfViewPluginBase::SetCaretPosition(const gfx::PointF& position) {
  engine()->SetCaretPosition(FrameToPdfCoordinates(position));
}

void PdfViewPluginBase::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  engine()->MoveRangeSelectionExtent(FrameToPdfCoordinates(extent));
}

void PdfViewPluginBase::SetSelectionBounds(const gfx::PointF& base,
                                           const gfx::PointF& extent) {
  engine()->SetSelectionBounds(FrameToPdfCoordinates(base),
                               FrameToPdfCoordinates(extent));
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
                                                  1 / (device_scale_ * zoom_));
  viewport_info.zoom = zoom_;
  viewport_info.scale = device_scale_;
  viewport_info.focus_info = {FocusObjectType::kNone, 0, 0};

  engine_->GetSelection(&viewport_info.selection_start_page_index,
                        &viewport_info.selection_start_char_index,
                        &viewport_info.selection_end_page_index,
                        &viewport_info.selection_end_char_index);

  SetAccessibilityViewportInfo(std::move(viewport_info));
}

void PdfViewPluginBase::SetZoom(double scale) {
  double old_zoom = zoom_;
  zoom_ = scale;

  OnGeometryChanged(old_zoom, device_scale_);
  if (!document_size_.IsEmpty())
    paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));
}

// static
base::Value::Dict PdfViewPluginBase::DictFromRect(const gfx::Rect& rect) {
  base::Value::Dict dict;
  dict.Set("x", rect.x());
  dict.Set("y", rect.y());
  dict.Set("width", rect.width());
  dict.Set("height", rect.height());
  return dict;
}

void PdfViewPluginBase::HandleDisplayAnnotationsMessage(
    const base::Value::Dict& message) {
  engine()->DisplayAnnotations(message.FindBool("display").value());
}

void PdfViewPluginBase::HandleGetNamedDestinationMessage(
    const base::Value::Dict& message) {
  absl::optional<PDFEngine::NamedDestination> named_destination =
      engine()->GetNamedDestination(*message.FindString("namedDestination"));

  const int page_number = named_destination.has_value()
                              ? base::checked_cast<int>(named_destination->page)
                              : -1;

  base::Value::Dict reply =
      PrepareReplyMessage("getNamedDestinationReply", message);
  reply.Set("pageNumber", page_number);

  if (named_destination.has_value() && !named_destination->view.empty()) {
    std::ostringstream view_stream;
    view_stream << named_destination->view;
    if (named_destination->xyz_params.empty()) {
      for (unsigned long i = 0; i < named_destination->num_params; ++i)
        view_stream << "," << named_destination->params[i];
    } else {
      view_stream << "," << named_destination->xyz_params;
    }

    reply.Set("namedDestinationView", view_stream.str());
  }

  SendMessage(std::move(reply));
}

void PdfViewPluginBase::HandleGetPasswordCompleteMessage(
    const base::Value::Dict& message) {
  DCHECK(password_callback_);
  std::move(password_callback_).Run(*message.FindString("password"));
}

void PdfViewPluginBase::HandleGetSelectedTextMessage(
    const base::Value::Dict& message) {
  // Always return unix newlines to JavaScript.
  std::string selected_text;
  base::RemoveChars(engine()->GetSelectedText(), "\r", &selected_text);

  base::Value::Dict reply =
      PrepareReplyMessage("getSelectedTextReply", message);
  reply.Set("selectedText", selected_text);
  SendMessage(std::move(reply));
}

void PdfViewPluginBase::HandleGetThumbnailMessage(
    const base::Value::Dict& message) {
  const int page_index = message.FindInt("page").value();
  base::Value::Dict reply = PrepareReplyMessage("getThumbnailReply", message);

  engine()->RequestThumbnail(page_index, device_scale_,
                             base::BindOnce(&PdfViewPluginBase::SendThumbnail,
                                            GetWeakPtr(), std::move(reply)));
}

void PdfViewPluginBase::HandleLoadPreviewPageMessage(
    const base::Value::Dict& message) {
  const std::string& url = *message.FindString("url");
  int index = message.FindInt("index").value();

  // For security reasons, crash if `url` is not for Print Preview.
  CHECK(IsPrintPreview());
  CHECK(IsPrintPreviewUrl(url));
  ProcessPreviewPageInfo(url, index);
}

void PdfViewPluginBase::HandlePrintMessage(
    const base::Value::Dict& /*message*/) {
  Print();
}

void PdfViewPluginBase::HandleResetPrintPreviewModeMessage(
    const base::Value::Dict& message) {
  const std::string& url = *message.FindString("url");
  bool is_grayscale = message.FindBool("grayscale").value();
  int print_preview_page_count = message.FindInt("pageCount").value();

  // For security reasons, crash if `url` is not for Print Preview.
  CHECK(IsPrintPreview());
  CHECK(IsPrintPreviewUrl(url));
  DCHECK_GE(print_preview_page_count, 0);

  // The page count is zero if the print preview source is a PDF. In which
  // case, the page index for `url` should be at `kCompletePDFIndex`.
  // When the page count is not zero, then the source is not PDF. In which
  // case, the page index for `url` should be non-negative.
  bool is_previewing_pdf = IsPreviewingPDF(print_preview_page_count);
  int page_index = ExtractPrintPreviewPageIndex(url);
  if (is_previewing_pdf)
    DCHECK_EQ(page_index, kCompletePDFIndex);
  else
    DCHECK_GE(page_index, 0);

  print_preview_page_count_ = print_preview_page_count;
  print_preview_loaded_page_count_ = 0;
  url_ = url;
  preview_pages_info_ = base::queue<PreviewPageInfo>();
  preview_document_load_state_ = DocumentLoadState::kComplete;
  document_load_state_ = DocumentLoadState::kLoading;
  last_progress_sent_ = 0;
  LoadUrl(GetURL(), base::BindOnce(&PdfViewPluginBase::DidOpen, GetWeakPtr()));
  preview_engine_.reset();

  // TODO(crbug.com/1237952): Figure out a more consistent way to preserve
  // engine settings across a Print Preview reset.
  engine_ = CreateEngine(this, PDFiumFormFiller::ScriptOption::kNoJavaScript);
  engine()->ZoomUpdated(zoom_ * device_scale_);
  engine()->PageOffsetUpdated(available_area_.OffsetFromOrigin());
  engine()->PluginSizeUpdated(available_area_.size());
  engine()->SetGrayscale(is_grayscale);

  paint_manager_.InvalidateRect(gfx::Rect(plugin_rect().size()));
}

void PdfViewPluginBase::HandleRotateClockwiseMessage(
    const base::Value::Dict& /*message*/) {
  engine()->RotateClockwise();
}

void PdfViewPluginBase::HandleRotateCounterclockwiseMessage(
    const base::Value::Dict& /*message*/) {
  engine()->RotateCounterclockwise();
}

void PdfViewPluginBase::HandleSaveAttachmentMessage(
    const base::Value::Dict& message) {
  const int index = message.FindInt("attachmentIndex").value();

  const std::vector<DocumentAttachmentInfo>& list =
      engine()->GetDocumentAttachmentInfoList();
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<size_t>(index), list.size());
  DCHECK(list[index].is_readable);
  DCHECK(IsSaveDataSizeValid(list[index].size_bytes));

  std::vector<uint8_t> data = engine()->GetAttachmentData(index);
  base::Value data_to_save(
      IsSaveDataSizeValid(data.size()) ? data : std::vector<uint8_t>());

  base::Value::Dict reply = PrepareReplyMessage("saveAttachmentReply", message);
  reply.Set("dataToSave", std::move(data_to_save));
  SendMessage(std::move(reply));
}

void PdfViewPluginBase::HandleSelectAllMessage(
    const base::Value::Dict& /*message*/) {
  engine()->SelectAll();
}

void PdfViewPluginBase::HandleSetPresentationModeMessage(
    const base::Value::Dict& message) {
  engine()->SetReadOnly(message.FindBool("enablePresentationMode").value());
}

void PdfViewPluginBase::HandleSetTwoUpViewMessage(
    const base::Value::Dict& message) {
  engine()->SetTwoUpView(message.FindBool("enableTwoUpView").value());
}

void PdfViewPluginBase::HandleStopScrollingMessage(
    const base::Value::Dict& /*message*/) {
  stop_scrolling_ = true;
}

void PdfViewPluginBase::HandleViewportMessage(
    const base::Value::Dict& message) {
  const base::Value::Dict* layout_options_value =
      message.FindDict("layoutOptions");
  if (layout_options_value) {
    DocumentLayout::Options layout_options;
    layout_options.FromValue(*layout_options_value);

    ui_direction_ = layout_options.direction();

    // TODO(crbug.com/1013800): Eliminate need to get document size from here.
    document_size_ = engine()->ApplyDocumentLayout(layout_options);

    OnGeometryChanged(zoom_, device_scale_);
    if (!document_size_.IsEmpty())
      paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));

    // Send 100% loading progress only after initial layout negotiated.
    if (last_progress_sent_ < 100 &&
        document_load_state_ == DocumentLoadState::kComplete) {
      SendLoadingProgress(/*percentage=*/100);
    }
  }

  gfx::Vector2dF scroll_offset(*message.FindDouble("xOffset"),
                               *message.FindDouble("yOffset"));
  double new_zoom = *message.FindDouble("zoom");
  const PinchPhase pinch_phase =
      static_cast<PinchPhase>(*message.FindInt("pinchPhase"));

  received_viewport_message_ = true;
  stop_scrolling_ = false;
  const double zoom_ratio = new_zoom / zoom_;

  if (pinch_phase == PinchPhase::kStart) {
    scroll_offset_at_last_raster_ = scroll_offset;
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
    const double pinch_x = *message.FindDouble("pinchX");
    const double pinch_y = *message.FindDouble("pinchY");
    gfx::Point pinch_center(pinch_x, pinch_y);

    // Get the pinch vector which represents the panning caused by the change in
    // pinch center between the start and the end of the gesture.
    const double pinch_vector_x = *message.FindDouble("pinchVectorX");
    const double pinch_vector_y = *message.FindDouble("pinchVectorY");
    gfx::Vector2d pinch_vector =
        gfx::Vector2d(pinch_vector_x * zoom_ratio, pinch_vector_y * zoom_ratio);

    gfx::Vector2d scroll_delta;
    // If the rendered document doesn't fill the display area we will
    // use `paint_offset` to anchor the paint vertically into the same place.
    // We use the scroll bars instead of the pinch vector to get the actual
    // position on screen of the paint.
    gfx::Vector2d paint_offset;

    if (plugin_rect_.width() > GetDocumentPixelWidth() * zoom_ratio) {
      // We want to keep the paint in the middle but it must stay in the same
      // position relative to the scroll bars.
      paint_offset = gfx::Vector2d(0, (1 - zoom_ratio) * pinch_center.y());
      scroll_delta = gfx::Vector2d(
          0,
          (scroll_offset.y() - scroll_offset_at_last_raster_.y() * zoom_ratio));

      pinch_vector = gfx::Vector2d();
      last_bitmap_smaller_ = true;
    } else if (last_bitmap_smaller_) {
      // When the document width covers the display area's width, we will anchor
      // the scroll bars disregarding where the actual pinch certer is.
      pinch_center = gfx::Point((plugin_rect_.width() / device_scale_) / 2,
                                (plugin_rect_.height() / device_scale_) / 2);
      const double zoom_when_doc_covers_plugin_width =
          zoom_ * plugin_rect_.width() / GetDocumentPixelWidth();
      paint_offset = gfx::Vector2d(
          (1 - new_zoom / zoom_when_doc_covers_plugin_width) * pinch_center.x(),
          (1 - zoom_ratio) * pinch_center.y());
      pinch_vector = gfx::Vector2d();
      scroll_delta = gfx::Vector2d(
          (scroll_offset.x() - scroll_offset_at_last_raster_.x() * zoom_ratio),
          (scroll_offset.y() - scroll_offset_at_last_raster_.y() * zoom_ratio));
    }

    paint_manager_.SetTransform(zoom_ratio, pinch_center,
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
    paint_manager_.ClearTransform();
    last_bitmap_smaller_ = false;
    needs_reraster_ = true;

    // If we're rerastering due to zooming out, we need to update the scroll
    // offset for the last raster, in case the user continues the gesture by
    // zooming in.
    scroll_offset_at_last_raster_ = scroll_offset;
  }

  // Bound the input parameters.
  new_zoom = std::max(kMinZoom, new_zoom);
  DCHECK(message.FindBool("userInitiated").has_value());

  SetZoom(new_zoom);
  UpdateScroll(GetScrollPositionFromOffset(scroll_offset));
}

void PdfViewPluginBase::DoPaint(const std::vector<gfx::Rect>& paint_rects,
                                std::vector<PaintReadyRect>& ready,
                                std::vector<gfx::Rect>& pending) {
  if (image_data_.drawsNothing()) {
    DCHECK(plugin_rect_.IsEmpty());
    return;
  }

  PrepareForFirstPaint(ready);

  if (!received_viewport_message_ || !needs_reraster_)
    return;

  engine()->PrePaint();

  std::vector<gfx::Rect> ready_rects;
  for (const gfx::Rect& paint_rect : paint_rects) {
    // Intersect with plugin area since there could be pending invalidates from
    // when the plugin area was larger.
    gfx::Rect rect =
        gfx::IntersectRects(paint_rect, gfx::Rect(plugin_rect_.size()));
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
          rect, gfx::Rect(gfx::Size(plugin_rect_.width(), first_page_ypos)));
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

void PdfViewPluginBase::PrepareForFirstPaint(
    std::vector<PaintReadyRect>& ready) {
  if (!first_paint_)
    return;

  // Fill the image data buffer with the background color.
  first_paint_ = false;
  image_data_.eraseColor(GetBackgroundColor());
  ready.emplace_back(gfx::SkIRectToRect(image_data_.bounds()),
                     image_data_.asImage(), /*flush_now=*/true);
}

void PdfViewPluginBase::ClearDeferredInvalidates() {
  DCHECK(!in_paint_);
  for (const gfx::Rect& rect : deferred_invalidates_)
    Invalidate(rect);
  deferred_invalidates_.clear();
}

void PdfViewPluginBase::SendThumbnail(base::Value::Dict reply,
                                      Thumbnail thumbnail) {
  DCHECK_EQ(*reply.FindString("type"), "getThumbnailReply");
  DCHECK(reply.FindString("messageId"));

  reply.Set("imageData", thumbnail.TakeData());
  reply.Set("width", thumbnail.image_size().width());
  reply.Set("height", thumbnail.image_size().height());
  SendMessage(std::move(reply));
}

void PdfViewPluginBase::LoadAccessibility() {
  accessibility_state_ = AccessibilityState::kLoaded;

  // A new document layout will trigger the creation of a new accessibility
  // tree, so `next_accessibility_page_index_` should be reset to ignore
  // outdated asynchronous calls of PrepareAndSetAccessibilityPageInfo().
  next_accessibility_page_index_ = 0;
  SetAccessibilityDocInfo(GetAccessibilityDocInfo());

  // If the document contents isn't accessible, don't send anything more.
  if (!(engine_->HasPermission(DocumentPermission::kCopy) ||
        engine_->HasPermission(DocumentPermission::kCopyAccessible))) {
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

void PdfViewPluginBase::DidOpen(std::unique_ptr<UrlLoader> loader,
                                int32_t result) {
  if (result == kSuccess) {
    if (!engine()->HandleDocumentLoad(std::move(loader), GetURL())) {
      document_load_state_ = DocumentLoadState::kLoading;
      DocumentLoadFailed();
    }
  } else if (result != kErrorAborted) {
    DocumentLoadFailed();
  }
}

void PdfViewPluginBase::DidOpenPreview(std::unique_ptr<UrlLoader> loader,
                                       int32_t result) {
  DCHECK_EQ(result, kSuccess);
  preview_client_ = std::make_unique<PreviewModeClient>(this);
  preview_engine_ = CreateEngine(preview_client_.get(),
                                 PDFiumFormFiller::ScriptOption::kNoJavaScript);
  preview_engine_->PluginSizeUpdated({});
  preview_engine_->HandleDocumentLoad(std::move(loader), GetURL());
}

void PdfViewPluginBase::OnPrintPreviewLoaded() {
  // Scroll location is retained across document loads in print preview mode, so
  // there's no need to override the scroll position by scrolling again.
  if (IsPreviewingPDF(print_preview_page_count_)) {
    SendPrintPreviewLoadedNotification();
  } else {
    DCHECK_EQ(0, print_preview_loaded_page_count_);
    print_preview_loaded_page_count_ = 1;
    AppendBlankPrintPreviewPages();
  }

  OnGeometryChanged(0, 0);
  if (!document_size_.IsEmpty())
    paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));
}

void PdfViewPluginBase::AppendBlankPrintPreviewPages() {
  engine()->AppendBlankPages(print_preview_page_count_);
  LoadNextPreviewPage();
}

void PdfViewPluginBase::ProcessPreviewPageInfo(const std::string& url,
                                               int dest_page_index) {
  DCHECK(IsPrintPreview());
  DCHECK_GE(dest_page_index, 0);
  DCHECK_LT(dest_page_index, print_preview_page_count_);

  // Print Preview JS will send the loadPreviewPage message for every page,
  // including the first page in the print preview, which has already been
  // loaded when handing the resetPrintPreviewMode message. Just ignore it.
  if (dest_page_index == 0)
    return;

  int src_page_index = ExtractPrintPreviewPageIndex(url);
  DCHECK_GE(src_page_index, 0);

  preview_pages_info_.push(std::make_pair(url, dest_page_index));
  LoadAvailablePreviewPage();
}

void PdfViewPluginBase::LoadAvailablePreviewPage() {
  if (preview_pages_info_.empty() ||
      document_load_state_ != DocumentLoadState::kComplete ||
      preview_document_load_state_ == DocumentLoadState::kLoading) {
    return;
  }

  preview_document_load_state_ = DocumentLoadState::kLoading;
  const std::string& url = preview_pages_info_.front().first;

  // Note that `last_progress_sent_` is not reset for preview page loads.
  LoadUrl(url,
          base::BindOnce(&PdfViewPluginBase::DidOpenPreview, GetWeakPtr()));
}

void PdfViewPluginBase::LoadNextPreviewPage() {
  if (!preview_pages_info_.empty()) {
    DCHECK_LT(print_preview_loaded_page_count_, print_preview_page_count_);
    LoadAvailablePreviewPage();
    return;
  }

  if (print_preview_loaded_page_count_ == print_preview_page_count_)
    SendPrintPreviewLoadedNotification();
}

gfx::Point PdfViewPluginBase::FrameToPdfCoordinates(
    const gfx::PointF& frame_coordinates) const {
  // TODO(crbug.com/1288847): Use methods on `blink::WebPluginContainer`.
  return gfx::ToFlooredPoint(
             gfx::ScalePoint(frame_coordinates, device_scale_)) -
         gfx::Vector2d(available_area_.x(), 0);
}

}  // namespace chrome_pdf
