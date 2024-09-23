// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils/coordinates.h"

#include <math.h>
#include <algorithm>

#include "base/check_op.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace chrome_pdf {
namespace draw_utils {

IndexedPage::IndexedPage(int index, const gfx::Rect& rect)
    : index(index), rect(rect) {}

IndexedPage::IndexedPage(const IndexedPage& other) = default;

IndexedPage& IndexedPage::operator=(const IndexedPage& other) = default;

IndexedPage::~IndexedPage() = default;

void AdjustBottomGapForRightSidePage(int page_x, gfx::Rect* bottom_gap) {
  bottom_gap->set_x(page_x);
  bottom_gap->set_width(bottom_gap->width() / 2);
}

void CenterRectHorizontally(int doc_width, gfx::Rect* rect) {
  DCHECK_GE(doc_width, rect->width());

  rect->set_x((doc_width - rect->width()) / 2);
}

void ExpandDocumentSize(const gfx::Size& rect_size, gfx::Size* doc_size) {
  int width_diff = std::max(0, rect_size.width() - doc_size->width());
  doc_size->Enlarge(width_diff, rect_size.height());
}

gfx::Rect GetBottomGapBetweenRects(int page_rect_bottom,
                                   const gfx::Rect& bottom_rect) {
  if (page_rect_bottom >= bottom_rect.bottom())
    return gfx::Rect(0, 0, 0, 0);

  return gfx::Rect(bottom_rect.x(), page_rect_bottom, bottom_rect.width(),
                   bottom_rect.bottom() - page_rect_bottom);
}

int GetMostVisiblePage(const std::vector<IndexedPage>& visible_pages,
                       const gfx::Rect& visible_screen) {
  if (visible_pages.empty())
    return -1;

  int most_visible_page_index = visible_pages.front().index;
  float most_visible_page_area = 0;
  for (const auto& visible_page : visible_pages) {
    float page_area = gfx::SizeF(visible_page.rect.size()).GetArea();
    // TODO(thestig): Check whether we can remove this check.
    if (page_area <= 0)
      continue;

    gfx::Rect screen_intersect =
        gfx::IntersectRects(visible_screen, visible_page.rect);
    float intersect_area =
        gfx::SizeF(screen_intersect.size()).GetArea() / page_area;
    if (intersect_area > most_visible_page_area) {
      most_visible_page_index = visible_page.index;
      most_visible_page_area = intersect_area;
    }
  }

  return most_visible_page_index;
}

gfx::Insets GetPageInsetsForTwoUpView(size_t page_index,
                                      size_t num_of_pages,
                                      const gfx::Insets& single_view_insets,
                                      int horizontal_separator) {
  DCHECK_LT(page_index, num_of_pages);

  // Don't change `two_up_insets` if the page is on the left side and is the
  // last page. In this case, the shadows on both sides should be the same size.
  gfx::Insets two_up_insets = single_view_insets;
  if (page_index % 2 == 1) {
    two_up_insets.set_left(horizontal_separator);
  } else if (page_index != num_of_pages - 1) {
    two_up_insets.set_right(horizontal_separator);
  }

  return two_up_insets;
}

gfx::Rect GetRectForSingleView(const gfx::Size& rect_size,
                               const gfx::Size& document_size) {
  gfx::Rect page_rect({0, document_size.height()}, rect_size);
  CenterRectHorizontally(document_size.width(), &page_rect);
  return page_rect;
}

gfx::Rect GetScreenRect(const gfx::Rect& rect,
                        const gfx::Point& position,
                        double zoom) {
  DCHECK_GT(zoom, 0);

  int x = static_cast<int>(rect.x() * zoom - position.x());
  int y = static_cast<int>(rect.y() * zoom - position.y());
  int right = static_cast<int>(ceil(rect.right() * zoom - position.x()));
  int bottom = static_cast<int>(ceil(rect.bottom() * zoom - position.y()));
  return gfx::Rect(x, y, right - x, bottom - y);
}

gfx::Rect GetSurroundingRect(int page_y,
                             int page_height,
                             const gfx::Insets& insets,
                             int doc_width,
                             int bottom_separator) {
  return gfx::Rect(
      0, page_y - insets.top(), doc_width,
      page_height + insets.top() + insets.bottom() + bottom_separator);
}

gfx::Rect GetLeftFillRect(const gfx::Rect& page_rect,
                          const gfx::Insets& insets,
                          int bottom_separator) {
  DCHECK_GE(page_rect.x(), insets.left());

  return gfx::Rect(
      0, page_rect.y() - insets.top(), page_rect.x() - insets.left(),
      page_rect.height() + insets.top() + insets.bottom() + bottom_separator);
}

gfx::Rect GetRightFillRect(const gfx::Rect& page_rect,
                           const gfx::Insets& insets,
                           int doc_width,
                           int bottom_separator) {
  int right_gap_x = page_rect.right() + insets.right();
  DCHECK_GE(doc_width, right_gap_x);

  return gfx::Rect(
      right_gap_x, page_rect.y() - insets.top(), doc_width - right_gap_x,
      page_rect.height() + insets.top() + insets.bottom() + bottom_separator);
}

gfx::Rect GetBottomFillRect(const gfx::Rect& page_rect,
                            const gfx::Insets& insets,
                            int bottom_separator) {
  return gfx::Rect(
      page_rect.x() - insets.left(), page_rect.bottom() + insets.bottom(),
      page_rect.width() + insets.left() + insets.right(), bottom_separator);
}

gfx::Rect GetLeftRectForTwoUpView(const gfx::Size& rect_size,
                                  const gfx::Point& position) {
  DCHECK_LE(rect_size.width(), position.x());

  return gfx::Rect(position.x() - rect_size.width(), position.y(),
                   rect_size.width(), rect_size.height());
}

gfx::Rect GetRightRectForTwoUpView(const gfx::Size& rect_size,
                                   const gfx::Point& position) {
  return gfx::Rect(position, rect_size);
}

}  // namespace draw_utils
}  // namespace chrome_pdf
