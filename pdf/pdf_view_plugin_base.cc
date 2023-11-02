// Copyright 2020 The Chromium Authors
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

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
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

void PdfViewPluginBase::DocumentLoadComplete() {
  DCHECK_EQ(DocumentLoadState::kLoading, document_load_state());
  set_document_load_state(DocumentLoadState::kComplete);

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
  DCHECK_EQ(DocumentLoadState::kLoading, document_load_state());
  set_document_load_state(DocumentLoadState::kFailed);

  UserMetricsRecordAction("PDF.LoadFailure");

  // Send a progress value of -1 to indicate a failure.
  SendLoadingProgress(-1);

  DidStopLoading();

  paint_manager().InvalidateRect(gfx::Rect(plugin_rect().size()));
}

void PdfViewPluginBase::SelectionChanged(const gfx::Rect& left,
                                         const gfx::Rect& right) {
  gfx::PointF left_point(left.x() + available_area().x(), left.y());
  gfx::PointF right_point(right.x() + available_area().x(), right.y());

  const float inverse_scale = 1.0f / device_scale();
  left_point.Scale(inverse_scale);
  right_point.Scale(inverse_scale);

  NotifySelectionChanged(left_point, left.height() * inverse_scale, right_point,
                         right.height() * inverse_scale);

  if (accessibility_state() == AccessibilityState::kLoaded)
    PrepareAndSetAccessibilityViewportInfo();
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

void PdfViewPluginBase::PrepareAndSetAccessibilityPageInfo(int32_t page_index) {
  // Outdated calls are ignored.
  if (page_index != next_accessibility_page_index())
    return;
  increment_next_accessibility_page_index();

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
  viewport_info.offset = gfx::ScaleToFlooredPoint(
      available_area().origin(), 1 / (device_scale() * zoom()));
  viewport_info.zoom = zoom();
  viewport_info.scale = device_scale();
  viewport_info.focus_info = {FocusObjectType::kNone, 0, 0};

  engine()->GetSelection(&viewport_info.selection_start_page_index,
                         &viewport_info.selection_start_char_index,
                         &viewport_info.selection_end_page_index,
                         &viewport_info.selection_end_char_index);

  SetAccessibilityViewportInfo(std::move(viewport_info));
}

void PdfViewPluginBase::LoadAccessibility() {
  set_accessibility_state(AccessibilityState::kLoaded);

  // A new document layout will trigger the creation of a new accessibility
  // tree, so `next_accessibility_page_index_` should be reset to ignore
  // outdated asynchronous calls of PrepareAndSetAccessibilityPageInfo().
  reset_next_accessibility_page_index();
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

}  // namespace chrome_pdf
