// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DOCUMENT_LAYOUT_H_
#define PDF_DOCUMENT_LAYOUT_H_

#include <cstddef>
#include <vector>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/values.h"
#include "pdf/page_orientation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

// Layout of pages within a PDF document. Pages are placed as rectangles
// (possibly rotated) in a non-overlapping vertical sequence.
//
// All layout units are pixels.
//
// The `Options` class controls the behavior of the layout, such as the default
// orientation of pages.
class DocumentLayout final {
 public:
  // TODO(crbug.com/40155509): Add `kTwoUpEven` page spread support.
  enum class PageSpread {
    kOneUp = 0,     // One page per spread.
    kTwoUpOdd = 1,  // Two pages per spread, with odd pages first.
  };

  // Options controlling layout behavior.
  class Options final {
   public:
    Options();

    Options(const Options& other);
    Options& operator=(const Options& other);

    ~Options();

    friend bool operator==(const Options& lhs, const Options& rhs) {
      return lhs.direction() == rhs.direction() &&
             lhs.default_page_orientation() == rhs.default_page_orientation() &&
             lhs.page_spread() == rhs.page_spread();
    }

    friend bool operator!=(const Options& lhs, const Options& rhs) {
      return !(lhs == rhs);
    }

    // Serializes layout options to a base::Value::Dict.
    base::Value::Dict ToValue() const;

    // Deserializes layout options from a base::Value::Dict.
    void FromValue(const base::Value::Dict& value);

    // Page layout direction. This is tied to the direction of the user's UI,
    // rather than the direction of individual pages.
    base::i18n::TextDirection direction() const { return direction_; }

    void set_direction(base::i18n::TextDirection direction) {
      direction_ = direction;
    }

    PageOrientation default_page_orientation() const {
      return default_page_orientation_;
    }

    // Rotates default page orientation 90 degrees clockwise.
    void RotatePagesClockwise();

    // Rotates default page orientation 90 degrees counterclockwise.
    void RotatePagesCounterclockwise();

    PageSpread page_spread() const { return page_spread_; }

    // Changes two-up view status.
    void set_page_spread(PageSpread spread) { page_spread_ = spread; }

   private:
    base::i18n::TextDirection direction_ = base::i18n::UNKNOWN_DIRECTION;
    PageOrientation default_page_orientation_ = PageOrientation::kOriginal;
    PageSpread page_spread_ = PageSpread::kOneUp;
  };

  static constexpr gfx::Insets kSingleViewInsets =
      gfx::Insets::TLBR(/*top=*/3, /*left=*/5, /*bottom=*/7, /*right=*/5);
  static constexpr int32_t kBottomSeparator = 4;
  static constexpr int32_t kHorizontalSeparator = 1;

  DocumentLayout();

  DocumentLayout(const DocumentLayout& other) = delete;
  DocumentLayout& operator=(const DocumentLayout& other) = delete;

  ~DocumentLayout();

  // Returns the layout options.
  const Options& options() const { return options_; }

  // Sets the layout options. If certain options with immediate effect change
  // (such as the default page orientation), the layout will be marked dirty.
  //
  // TODO(kmoon): We shouldn't have layout options that take effect immediately.
  void SetOptions(const Options& options);

  // Returns true if the layout has been modified since the last call to
  // clear_dirty(). The initial state is false (clean), which assumes
  // appropriate default behavior for an initially empty layout.
  bool dirty() const { return dirty_; }

  // Clears the dirty() state of the layout. This should be called after any
  // layout changes have been applied.
  void clear_dirty() { dirty_ = false; }

  // Returns the layout's total size.
  const gfx::Size& size() const { return size_; }

  size_t page_count() const { return page_layouts_.size(); }

  // Gets the layout rectangle for a page. Only valid after computing a layout.
  const gfx::Rect& page_rect(size_t page_index) const {
    DCHECK_LT(page_index, page_count());
    return page_layouts_[page_index].outer_rect;
  }

  // Gets the layout rectangle for a page's bounds (which excludes additional
  // regions like page shadows). Only valid after computing a layout.
  const gfx::Rect& page_bounds_rect(size_t page_index) const {
    DCHECK_LT(page_index, page_count());
    return page_layouts_[page_index].inner_rect;
  }

  // Computes the layout for a given list of `page_sizes` based on `options_`.
  void ComputeLayout(const std::vector<gfx::Size>& page_sizes);

 private:
  // Layout of a single page.
  struct PageLayout {
    // Bounding rectangle for the page with decorations.
    gfx::Rect outer_rect;

    // Bounding rectangle for the page without decorations.
    gfx::Rect inner_rect;
  };

  // Helpers for ComputeLayout() handling different page spreads.
  void ComputeOneUpLayout(const std::vector<gfx::Size>& page_sizes);
  void ComputeTwoUpOddLayout(const std::vector<gfx::Size>& page_sizes);

  // Copies `source_rect` to `destination_rect`, setting `dirty_` to true if
  // `destination_rect` is modified as a result.
  void CopyRectIfModified(const gfx::Rect& source_rect,
                          gfx::Rect& destination_rect);

  Options options_;

  // Indicates if the layout has changed in an externally-observable way,
  // usually as a result of calling `ComputeLayout()` with different inputs.
  //
  // Some operations that may trigger layout changes:
  // * Changing page sizes
  // * Adding or removing pages
  // * Changing page orientations
  bool dirty_ = false;

  // Layout's total size.
  gfx::Size size_;

  std::vector<PageLayout> page_layouts_;
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_LAYOUT_H_
