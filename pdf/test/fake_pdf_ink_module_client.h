// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_FAKE_PDF_INK_MODULE_CLIENT_H_
#define PDF_TEST_FAKE_PDF_INK_MODULE_CLIENT_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/values.h"
#include "pdf/buildflags.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_ink_ids.h"
#include "pdf/pdf_ink_module_client.h"
#include "pdf/pdf_ink_text.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

class FakePdfInkModuleClient : public PdfInkModuleClient {
 public:
  FakePdfInkModuleClient();
  FakePdfInkModuleClient(const FakePdfInkModuleClient&) = delete;
  FakePdfInkModuleClient& operator=(const FakePdfInkModuleClient&) = delete;
  ~FakePdfInkModuleClient() override;

  // PdfInkModuleClient:
  MOCK_METHOD(void,
              AddFont,
              (FontId font_id,
               const std::string&,
               base::span<const uint8_t> serialized_typeface),
              (override));

  MOCK_METHOD(void,
              DiscardStroke,
              (int page_index, InkStrokeId id),
              (override));

  MOCK_METHOD(void, DiscardText, (InkTextId id), (override));

  MOCK_METHOD(void,
              DrawText,
              (int page_index,
               InkTextId id,
               base::span<const InkTextLine> text_lines,
               float ascent,
               double pdf_zoom,
               const InkTextBoxAttributes& attributes),
              (override));

  MOCK_METHOD(void,
              ExtendSelectionByPoint,
              (const gfx::PointF& point),
              (override));

  MOCK_METHOD(gfx::Transform,
              GetCanonicalToPdfTransform,
              (int page_index),
              (override));

  MOCK_METHOD(ui::Cursor, GetCursor, (), (override));

  PageOrientation GetOrientation() const override;

  MOCK_METHOD(PdfCaret*, GetPdfCaret, (), (override));

  MOCK_METHOD(SelectionRectMap, GetSelectionRectMap, (), (override));

  gfx::Vector2dF GetViewportOriginOffset() override;

  gfx::Rect GetPageContentsRect(int page_index) override;

  gfx::SizeF GetPageSizeInPoints(int page_index) override;

  float GetZoom() const override;

  void Invalidate(const gfx::Rect& rect) override;

  MOCK_METHOD(bool,
              IsSelectableTextOrLinkArea,
              (const gfx::PointF& point),
              (override));

  MOCK_METHOD(DocumentInkTextBoxesMap,
              LoadTextAnnotationsFromPdf,
              (),
              (override));

  MOCK_METHOD(PdfInkModuleClient::DocumentV2InkPathShapesMap,
              LoadV2InkPathsFromPdf,
              (),
              (override));

  MOCK_METHOD(void,
              OnTextOrLinkAreaClick,
              (const gfx::PointF& point, int click_count),
              (override));

  MOCK_METHOD(void, PostMessage, (base::DictValue message), (override));

  MOCK_METHOD(void,
              RequestThumbnail,
              (int page_index, SendThumbnailCallback callback),
              (override));

  MOCK_METHOD(void,
              StrokeAdded,
              (int page_index, InkStrokeId id, const ink::Stroke& stroke),
              (override));

  void StrokeFinished(bool modified) override;

  void StrokeStarted() override;

  MOCK_METHOD(void, UpdateInkCursor, (const ui::Cursor&), (override));

  MOCK_METHOD(void,
              UpdateShapeActive,
              (int page_index, InkModeledShapeId id, bool active),
              (override));

  MOCK_METHOD(void,
              UpdateStrokeActive,
              (int page_index, InkStrokeId id, bool active),
              (override));

  MOCK_METHOD(void,
              UpdateTextActiveAndInvalidate,
              (TextId id, bool active),
              (override));

  int VisiblePageIndexFromPoint(const gfx::PointF& point) override;

  int stroke_started_count() const { return stroke_started_count_; }
  int modified_stroke_finished_count() const {
    return modified_stroke_finished_count_;
  }
  int unmodified_stroke_finished_count() const {
    return unmodified_stroke_finished_count_;
  }

  const std::vector<gfx::Rect>& invalidations() const { return invalidations_; }

  // Provide the sequence of pages and the coordinates and dimensions for how
  // they are laid out in a viewer plane.  It is upon the caller to ensure the
  // positioning makes sense (e.g., pages do not overlap).
  void set_page_layouts(base::span<const gfx::RectF> page_layouts);

  // Marks pages as visible or not. The caller is responsible for making sure
  // the values makes sense.
  void set_page_visibility(int index, bool visible);

  void set_orientation(PageOrientation orientation) {
    orientation_ = orientation;
  }

  void set_viewport_origin_offset(const gfx::Vector2dF& offset) {
    viewport_origin_offset_ = offset;
  }

  void set_zoom(float zoom) { zoom_ = zoom; }

 private:
  int stroke_started_count_ = 0;
  int modified_stroke_finished_count_ = 0;
  int unmodified_stroke_finished_count_ = 0;
  std::vector<gfx::RectF> page_layouts_;
  std::set<int> visible_page_indices_;
  PageOrientation orientation_ = PageOrientation::kOriginal;
  gfx::Vector2dF viewport_origin_offset_;
  float zoom_ = 1.0f;
  std::vector<gfx::Rect> invalidations_;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_FAKE_PDF_INK_MODULE_CLIENT_H_
