// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_layout.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/values.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

constexpr char kDefaultPageOrientation[] = "defaultPageOrientation";
constexpr char kTwoUpViewEnabled[] = "twoUpViewEnabled";

int GetWidestPageWidth(const std::vector<gfx::Size>& page_sizes) {
  int widest_page_width = 0;
  for (const auto& page_size : page_sizes) {
    widest_page_width = std::max(widest_page_width, page_size.width());
  }

  return widest_page_width;
}

gfx::Rect InsetRect(const gfx::Rect& rect,
                    const draw_utils::PageInsetSizes& inset_sizes) {
  gfx::Rect inset_rect(rect);
  inset_rect.Inset(inset_sizes.left, inset_sizes.top, inset_sizes.right,
                   inset_sizes.bottom);
  return inset_rect;
}

}  // namespace

const draw_utils::PageInsetSizes DocumentLayout::kSingleViewInsets{
    /*left=*/5, /*top=*/3, /*right=*/5, /*bottom=*/7};

DocumentLayout::Options::Options() = default;

DocumentLayout::Options::Options(const Options& other) = default;
DocumentLayout::Options& DocumentLayout::Options::operator=(
    const Options& other) = default;

DocumentLayout::Options::~Options() = default;

base::Value DocumentLayout::Options::ToValue() const {
  base::Value dictionary(base::Value::Type::DICTIONARY);
  dictionary.SetIntKey(kDefaultPageOrientation,
                       static_cast<int32_t>(default_page_orientation_));
  dictionary.SetBoolKey(kTwoUpViewEnabled, two_up_view_enabled_);
  return dictionary;
}

void DocumentLayout::Options::FromValue(const base::Value& value) {
  DCHECK(value.is_dict());

  int32_t default_page_orientation =
      value.FindKey(kDefaultPageOrientation)->GetInt();
  DCHECK_GE(default_page_orientation,
            static_cast<int32_t>(PageOrientation::kOriginal));
  DCHECK_LE(default_page_orientation,
            static_cast<int32_t>(PageOrientation::kLast));
  default_page_orientation_ =
      static_cast<PageOrientation>(default_page_orientation);

  two_up_view_enabled_ = value.FindKey(kTwoUpViewEnabled)->GetBool();
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

void DocumentLayout::ComputeSingleViewLayout(
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
    CopyRectIfModified(InsetRect(page_rect, kSingleViewInsets),
                       page_layouts_[i].inner_rect);

    draw_utils::ExpandDocumentSize(page_size, &document_size);
  }

  if (size_ != document_size) {
    size_ = document_size;
    dirty_ = true;
  }
}

void DocumentLayout::ComputeTwoUpViewLayout(
    const std::vector<gfx::Size>& page_sizes) {
  gfx::Size document_size(GetWidestPageWidth(page_sizes), 0);

  if (page_layouts_.size() != page_sizes.size()) {
    // TODO(kmoon): May want to do less work when shrinking a layout.
    page_layouts_.resize(page_sizes.size());
    dirty_ = true;
  }

  for (size_t i = 0; i < page_sizes.size(); ++i) {
    draw_utils::PageInsetSizes page_insets =
        draw_utils::GetPageInsetsForTwoUpView(
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
    CopyRectIfModified(InsetRect(page_rect, page_insets),
                       page_layouts_[i].inner_rect);
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
