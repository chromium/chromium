// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DRAW_UTILS_COORDINATES_H_
#define PDF_DRAW_UTILS_COORDINATES_H_

#include <stddef.h>

#include <vector>

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Point;
class Size;
}  // namespace gfx

namespace chrome_pdf {
namespace draw_utils {

// Struct for sending a page's gfx::Rect object along with its corresponding
// index in the PDF document.
struct IndexedPage {
  IndexedPage(int index, const gfx::Rect& rect);
  IndexedPage(const IndexedPage& other);
  IndexedPage& operator=(const IndexedPage& other);
  ~IndexedPage();

  int index;
  gfx::Rect rect;
};

// Given a right page's `bottom_gap`, reduce it to only the part of `bottom_gap`
// that is directly below the right page by translating `bottom_gap` to `page_x`
// and halving its width. This avoids over-drawing empty space on the already
// drawn left page and the empty space to the right of the page.
void AdjustBottomGapForRightSidePage(int page_x, gfx::Rect* bottom_gap);

// Given `doc_width`, horizontally center `rect` within the document.
// `doc_width` must be greater than or equal to `rect`.
void CenterRectHorizontally(int doc_width, gfx::Rect* rect);

// Given `rect_size`, sets the width of `doc_size` to the max of `rect_size`'s
// width and `doc_size`'s width. Also adds the height of `rect_size` to
// `doc_size`'s height.
void ExpandDocumentSize(const gfx::Size& rect_size, gfx::Size* doc_size);

// Given `page_rect_bottom` and `bottom_rect` in the same coordinate space,
// return a gfx::Rect object representing the portion of `bottom_rect` that is
// below `page_rect_bottom`. Returns an empty rectangle if `page_rect_bottom`
// is greater than or equal to `bottom_rect.bottom()`.
gfx::Rect GetBottomGapBetweenRects(int page_rect_bottom,
                                   const gfx::Rect& bottom_rect);

// Given `visible_pages` and `visible_screen` in the same coordinates, return
// the index of the page in `visible_pages` which has the largest proportion of
// its area intersecting with `visible_screen`. If there is a tie, return the
// page with the lower index. Returns -1 if `visible_pages` is empty. Returns
// first page in `visible_pages` if no page intersects with `visible_screen`.
int GetMostVisiblePage(const std::vector<IndexedPage>& visible_pages,
                       const gfx::Rect& visible_screen);

// Given `page_index`, and `num_of_pages`, return the configuration of
// `single_view_insets` and `horizontal_separator` for the current page in
// two-up view.
gfx::Insets GetPageInsetsForTwoUpView(size_t page_index,
                                      size_t num_of_pages,
                                      const gfx::Insets& single_view_insets,
                                      int horizontal_separator);

// Given `rect_size` and `document_size` create a horizontally centered
// gfx::Rect placed at the bottom of the current document.
gfx::Rect GetRectForSingleView(const gfx::Size& rect_size,
                               const gfx::Size& document_size);

// Given `rect` in document coordinates, a `position` in screen coordinates,
// and a `zoom` factor, returns the rectangle in screen coordinates (i.e.
// 0,0 is top left corner of plugin area). An empty `rect` will always
// result in an empty output rect. For `zoom`, a value of 1 means 100%.
// `zoom` is never less than or equal to 0.
gfx::Rect GetScreenRect(const gfx::Rect& rect,
                        const gfx::Point& position,
                        double zoom);

// Given `page_y`, `page_height`, `insets`, `doc_width`, and `bottom_separator`
// all in the same coordinate space, return the page and its surrounding border
// areas and `bottom_separator`. This includes the sides if the page is narrower
// than the document.
gfx::Rect GetSurroundingRect(int page_y,
                             int page_height,
                             const gfx::Insets& insets,
                             int doc_width,
                             int bottom_separator);

// Given `page_rect` in document coordinates, `insets`, and `bottom_separator`,
// return a gfx::Rect object representing the gap on the left side of the page
// created by insetting the page. I.e. the difference, on the left side, between
// the initial `page_rect` and the `page_rect` inset with `insets` (current
// value of `page_rect`). The x coordinate of `page_rect` must be greater than
// or equal to `insets.left`.
gfx::Rect GetLeftFillRect(const gfx::Rect& page_rect,
                          const gfx::Insets& insets,
                          int bottom_separator);

// Same as GetLeftFillRect(), but for the right side of `page_rect` and also
// depends on the `doc_width`. Additionally, `doc_width` must be greater than or
// equal to the sum of `page_rect.right` and `insets.right`.
gfx::Rect GetRightFillRect(const gfx::Rect& page_rect,
                           const gfx::Insets& insets,
                           int doc_width,
                           int bottom_separator);

// Same as GetLeftFillRect(), but for the bottom side of `page_rect`.
gfx::Rect GetBottomFillRect(const gfx::Rect& page_rect,
                            const gfx::Insets& insets,
                            int bottom_separator);

// Given `rect_size`, create a gfx::Rect where the top-right corner lies at
// `position`. The width of `rect_size` must be less than or equal to the x
// value for `position`.
gfx::Rect GetLeftRectForTwoUpView(const gfx::Size& rect_size,
                                  const gfx::Point& position);

// Given `rect_size`, create a gfx::Rect where the top-left corner lies at
// `position`.
gfx::Rect GetRightRectForTwoUpView(const gfx::Size& rect_size,
                                   const gfx::Point& position);

}  // namespace draw_utils
}  // namespace chrome_pdf

#endif  // PDF_DRAW_UTILS_COORDINATES_H_
