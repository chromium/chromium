// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_layout.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/values.h"
#include "pdf/draw_utils/coordinates.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

constexpr char kDirection[] = "direction";
constexpr char kDefaultPageOrientation[] = "defaultPageOrientation";
constexpr char kTwoUpViewEnabled[] = "twoUpViewEnabled";

int GetWidestPageWidth(const std::vector<gfx::Size>& page_sizes) {
  int widest_page_width = 0;
  for (const auto& page_size : page_sizes) {
    widest_page_width = std::max(widest_page_width, page_size.width());
  }

  return widest_page_width;
}

}  // namespace

DocumentLayout::Options::Options() = default;

DocumentLayout::Options::Options(const Options& other) = default;
DocumentLayout::Options& DocumentLayout::Options::operator=(
    const Options& other) = default;

DocumentLayout::Options::~Options() = default;

base::Value::Dict DocumentLayout::Options::ToValue() const {
  base::Value::Dict dictionary;
  dictionary.Set(kDirection, direction_);
  dictionary.Set(kDefaultPageOrientation,
                 static_cast<int>(default_page_orientation_));
  dictionary.Set(kTwoUpViewEnabled, page_spread_ == PageSpread::kTwoUpOdd);
  return dictionary;
}

void DocumentLayout::Options::FromValue(const base::Value::Dict& value) {
  int32_t direction = value.FindInt(kDirection).value();
  DCHECK_GE(direction, base::i18n::UNKNOWN_DIRECTION);
  DCHECK_LE(direction, base::i18n::TEXT_DIRECTION_MAX);
  direction_ = static_cast<base::i18n::TextDirection>(direction);

  int32_t default_page_orientation =
      value.FindInt(kDefaultPageOrientation).value();
  DCHECK_GE(default_page_orientation,
            static_cast<int32_t>(PageOrientation::kOriginal));
  DCHECK_LE(default_page_orientation,
            static_cast<int32_t>(PageOrientation::kLast));
  default_page_orientation_ =
      static_cast<PageOrientation>(default_page_orientation);

  page_spread_ = value.FindBool(kTwoUpViewEnabled).value()
                     ? PageSpread::kTwoUpOdd
                     : PageSpread::kOneUp;
}

void DocumentLayout::Options::RotatePagesClockwise() {
  default_page_orientation_ = RotateClockwise(default_page_orientation_);
}

void DocumentLayout::Options::RotatePagesCounterclockwise() {
  default_page_orientation_ = RotateCounterclockwise(default_page_orientation_);
}

DocumentLayout::DocumentLayout() = default;

DocumentLayout::~DocumentLayout() = default;

void DocumentLayout::SetOptions(const Options& options) {
  // To be conservative, we want to consider the layout dirty for any layout
  // option changes, even if the page rects don't necessarily change when
  // layout options change.
  //
  // We also probably don't want layout changes to actually kick in until
  // the next call to ComputeLayout(). (In practice, we'll call ComputeLayout()
  // shortly after calling SetOptions().)
  if (options_ != options) {
    dirty_ = true;
  }
  options_ = options;
}

void DocumentLayout::ComputeLayout(const std::vector<gfx::Size>& page_sizes) {
  switch (options_.page_spread()) {
    case PageSpread::kOneUp:
      return ComputeOneUpLayout(page_sizes);
    case PageSpread::kTwoUpOdd:
      return ComputeTwoUpOddLayout(page_sizes);
  }
  NOTREACHED();
}

void DocumentLayout::ComputeOneUpLayout(
    const std::vector<gfx::Size>& page_sizes) {
  gfx::Size document_size(GetWidestPageWidth(page_sizes), 0);

  if (page_layouts_.size() != page_sizes.size()) {
    // TODO(kmoon): May want to do less work when shrinking a layout.
    page_layouts_.resize(page_sizes.size());
    dirty_ = true;
  }

  for (size_t i = 0; i < page_sizes.size(); ++i) {
    if (i != 0) {
      // Add space for bottom separator.
      document_size.Enlarge(0, kBottomSeparator);
    }

    const gfx::Size& page_size = page_sizes[i];
    gfx::Rect page_rect =
        draw_utils::GetRectForSingleView(page_size, document_size);
    CopyRectIfModified(page_rect, page_layouts_[i].outer_rect);
    page_rect.Inset(kSingleViewInsets);
    CopyRectIfModified(page_rect, page_layouts_[i].inner_rect);

    draw_utils::ExpandDocumentSize(page_size, &document_size);
  }

  if (size_ != document_size) {
    size_ = document_size;
    dirty_ = true;
  }
}

void DocumentLayout::ComputeTwoUpOddLayout(
    const std::vector<gfx::Size>& page_sizes) {
  gfx::Size document_size(GetWidestPageWidth(page_sizes), 0);

  if (page_layouts_.size() != page_sizes.size()) {
    // TODO(kmoon): May want to do less work when shrinking a layout.
    page_layouts_.resize(page_sizes.size());
    dirty_ = true;
  }

  for (size_t i = 0; i < page_sizes.size(); ++i) {
    gfx::Insets page_insets = draw_utils::GetPageInsetsForTwoUpView(
        i, page_sizes.size(), kSingleViewInsets, kHorizontalSeparator);
    const gfx::Size& page_size = page_sizes[i];

    gfx::Rect page_rect;
    if (i % 2 == 0) {
      page_rect = draw_utils::GetLeftRectForTwoUpView(
          page_size, {document_size.width(), document_size.height()});
    } else {
      page_rect = draw_utils::GetRightRectForTwoUpView(
          page_size, {document_size.width(), document_size.height()});
      document_size.Enlarge(
          0, std::max(page_size.height(), page_sizes[i - 1].height()));
    }
    CopyRectIfModified(page_rect, page_layouts_[i].outer_rect);
    page_rect.Inset(page_insets);
    CopyRectIfModified(page_rect, page_layouts_[i].inner_rect);
  }

  if (page_sizes.size() % 2 == 1) {
    document_size.Enlarge(0, page_sizes.back().height());
  }

  document_size.set_width(2 * document_size.width());

  if (size_ != document_size) {
    size_ = document_size;
    dirty_ = true;
  }
}

void DocumentLayout::CopyRectIfModified(const gfx::Rect& source_rect,
                                        gfx::Rect& destination_rect) {
  if (destination_rect != source_rect) {
    destination_rect = source_rect;
    dirty_ = true;
  }
}

}  // namespace chrome_pdf
