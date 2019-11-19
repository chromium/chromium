// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DRAW_UTILS_COORDINATES_H_
#define PDF_DRAW_UTILS_COORDINATES_H_

#include <stddef.h>

#include <vector>

#include "ppapi/cpp/rect.h"

namespace pp {
class Point;
}

namespace chrome_pdf {
namespace draw_utils {

// Struct for sending the sizes of the insets applied to the page to accommodate
// for shadows/separators. I.e. the left component corresponds to the amount a
// page was inset on the left side.
struct PageInsetSizes {
  int left;
  int top;
  int right;
  int bottom;
};

// Struct for sending a page's pp::Rect object along with its corresponding
// index in the PDF document.
struct IndexedPage {
  int index;
  pp::Rect rect;
};

// Given a right page's |bottom_gap|, reduce it to only the part of |bottom_gap|
// that is directly below the right page by translating |bottom_gap| to |page_x|
// and halving its width. This avoids over-drawing empty space on the already
// drawn left page and the empty space to the right of the page.
void AdjustBottomGapForRightSidePage(int page_x, pp::Rect* bottom_gap);

// Given |doc_width|, horizontally center |rect| within the document.
// |doc_width| must be greater than or equal to |rect|.
void CenterRectHorizontally(int doc_width, pp::Rect* rect);

// Given |rect_size|, sets the width of |doc_size| to the max of |rect_size|'s
// width and |doc_size|'s width. Also adds the height of |rect_size| to
// |doc_size|'s height.
void ExpandDocumentSize(const pp::Size& rect_size, pp::Size* doc_size);

// Given |page_rect_bottom| and |bottom_rect| in the same coordinate space,
// return a pp::Rect object representing the portion of |bottom_rect| that is
// below |page_rect_bottom|. Returns an empty rectangle if |page_rect_bottom|
// is greater than or equal to |bottom_rect.bottom()|.
pp::Rect GetBottomGapBetweenRects(int page_rect_bottom,
                                  const pp::Rect& bottom_rect);

// Given |visible_pages| and |visible_screen| in the same coordinates, return
// the index of the page in |visible_pages| which has the largest proportion of
// its area intersecting with |visible_screen|. If there is a tie, return the
// page with the lower index. Returns -1 if |visible_pages| is empty. Returns
// first page in |visible_pages| if no page intersects with |visible_screen|.
int GetMostVisiblePage(const std::vector<IndexedPage>& visible_pages,
                       const pp::Rect& visible_screen);

// Given |page_index|, and |num_of_pages|, return the configuration of
// |single_view_insets| and |horizontal_separator| for the current page in
// two-up view.
PageInsetSizes GetPageInsetsForTwoUpView(
    size_t page_index,
    size_t num_of_pages,
    const PageInsetSizes& single_view_insets,
    int horizontal_separator);

// Given |rect_size| and |document_size| create a horizontally centered
// pp::Rect placed at the bottom of the current document.
pp::Rect GetRectForSingleView(const pp::Size& rect_size,
                              const pp::Size& document_size);

// Given |rect| in document coordinates, a |position| in screen coordinates,
// and a |zoom| factor, returns the rectangle in screen coordinates (i.e.
// 0,0 is top left corner of plugin area). An empty |rect| will always
// result in an empty output rect. For |zoom|, a value of 1 means 100%.
// |zoom| is never less than or equal to 0.
pp::Rect GetScreenRect(const pp::Rect& rect,
                       const pp::Point& position,
                       double zoom);

// Given |page_y|, |page_height|, |inset_sizes|, |doc_width|, and
// |bottom_separator| all in the same coordinate space, return the page and its
// surrounding border areas and |bottom_separator|. This includes the sides if
// the page is narrower than the document.
pp::Rect GetSurroundingRect(int page_y,
                            int page_height,
                            const PageInsetSizes& inset_sizes,
                            int doc_width,
                            int bottom_separator);

// Given |page_rect| in document coordinates, |inset_sizes|, and
// |bottom_separator|, return a pp::Rect object representing the gap on the
// left side of the page created by insetting the page. I.e. the difference,
// on the left side, between the initial |page_rect| and the |page_rect| inset
// with |inset_sizes| (current value of |page_rect|).
// The x coordinate of |page_rect| must be greater than or equal to
// |inset_sizes.left|.
pp::Rect GetLeftFillRect(const pp::Rect& page_rect,
                         const PageInsetSizes& inset_sizes,
                         int bottom_separator);

// Same as GetLeftFillRect(), but for the right side of |page_rect| and also
// depends on the |doc_width|. Additionally, |doc_width| must be greater than or
// equal to the sum of |page_rect.right| and |inset_sizes.right|.
pp::Rect GetRightFillRect(const pp::Rect& page_rect,
                          const PageInsetSizes& inset_sizes,
                          int doc_width,
                          int bottom_separator);

// Same as GetLeftFillRect(), but for the bottom side of |page_rect|.
pp::Rect GetBottomFillRect(const pp::Rect& page_rect,
                           const PageInsetSizes& inset_sizes,
                           int bottom_separator);

// Given |rect_size|, create a pp::Rect where the top-right corner lies at
// |position|. The width of |rect_size| must be less than or equal to the x
// value for |position|.
pp::Rect GetLeftRectForTwoUpView(const pp::Size& rect_size,
                                 const pp::Point& position);

// Given |rect_size|, create a pp::Rect where the top-left corner lies at
// |position|.
pp::Rect GetRightRectForTwoUpView(const pp::Size& rect_size,
                                  const pp::Point& position);

}  // namespace draw_utils
}  // namespace chrome_pdf

#endif  // PDF_DRAW_UTILS_COORDINATES_H_
