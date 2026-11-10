// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/fake_pdf_ink_module_client.h"

#include <stddef.h>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "pdf/page_orientation.h"
#include "printing/units.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

FakePdfInkModuleClient::FakePdfInkModuleClient() = default;

FakePdfInkModuleClient::~FakePdfInkModuleClient() = default;

PageOrientation FakePdfInkModuleClient::GetOrientation() const {
  return orientation_;
}

gfx::Vector2dF FakePdfInkModuleClient::GetViewportOriginOffset() {
  return viewport_origin_offset_;
}

gfx::Rect FakePdfInkModuleClient::GetPageContentsRect(int page_index) {
  CHECK_GE(page_index, 0);
  CHECK_LT(static_cast<size_t>(page_index), page_layouts_.size());
  return gfx::ToEnclosedRect(page_layouts_[page_index]);
}

gfx::SizeF FakePdfInkModuleClient::GetPageSizeInPoints(int page_index) {
  CHECK_GE(page_index, 0);
  CHECK_LT(static_cast<size_t>(page_index), page_layouts_.size());
  gfx::SizeF page_size = page_layouts_[page_index].size();
  page_size.Scale(printing::kUnitConversionFactorPixelsToPoints);
  return page_size;
}

float FakePdfInkModuleClient::GetZoom() const {
  return zoom_;
}

void FakePdfInkModuleClient::Invalidate(const gfx::Rect& rect) {
  invalidations_.push_back(rect);
}

void FakePdfInkModuleClient::StrokeFinished(bool modified) {
  if (modified) {
    ++modified_stroke_finished_count_;
  } else {
    ++unmodified_stroke_finished_count_;
  }
}

void FakePdfInkModuleClient::StrokeStarted() {
  ++stroke_started_count_;
}

int FakePdfInkModuleClient::VisiblePageIndexFromPoint(
    const gfx::PointF& point) {
  for (size_t i = 0; i < page_layouts_.size(); ++i) {
    if (visible_page_indices_.contains(i) && page_layouts_[i].Contains(point)) {
      return i;
    }
  }

  // Point is not over a visible page in the viewer plane.
  return -1;
}

void FakePdfInkModuleClient::set_page_layouts(
    base::span<const gfx::RectF> page_layouts) {
  page_layouts_ = base::ToVector(page_layouts);
}

void FakePdfInkModuleClient::set_page_visibility(int index, bool visible) {
  if (visible) {
    visible_page_indices_.insert(index);
  } else {
    visible_page_indices_.erase(index);
  }
}

}  // namespace chrome_pdf
