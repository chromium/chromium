// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_number.h"

#include <algorithm>
#include <limits>

#include "base/check_op.h"
#include "printing/print_job_constants.h"

namespace printing {

PageNumber::PageNumber(const PageRanges& ranges, uint32_t document_page_count) {
  Init(ranges, document_page_count);
}

PageNumber::PageNumber()
    : ranges_(nullptr),
      page_number_(kInvalidPageIndex),
      page_range_index_(kInvalidPageIndex),
      document_page_count_(0) {}

PageNumber::PageNumber(const PageNumber& other) = default;

PageNumber& PageNumber::operator=(const PageNumber& other) = default;

void PageNumber::Init(const PageRanges& ranges, uint32_t document_page_count) {
  DCHECK(document_page_count);
  ranges_ = ranges.empty() ? nullptr : &ranges;
  document_page_count_ = document_page_count;
  if (ranges_) {
    uint32_t first_page = (*ranges_)[0].from;
    if (first_page < document_page_count) {
      page_range_index_ = 0;
      page_number_ = (*ranges_)[0].from;
    } else {
      page_range_index_ = kInvalidPageIndex;
      page_number_ = kInvalidPageIndex;
    }
  } else {
    page_range_index_ = kInvalidPageIndex;
    page_number_ = 0;
  }
}

uint32_t PageNumber::operator++() {
  ++page_number_;
  if (page_number_ >= document_page_count_) {
    // Finished.
    *this = npos();
  } else if (ranges_ && page_number_ > (*ranges_)[page_range_index_].to) {
    DCHECK_LE(ranges_->size(),
              static_cast<size_t>(std::numeric_limits<int>::max()));
    if (++page_range_index_ == ranges_->size()) {
      // Finished.
      *this = npos();
    } else {
      page_number_ = (*ranges_)[page_range_index_].from;
      if (page_number_ >= document_page_count_) {
        // Finished.
        *this = npos();
      }
    }
  }
  return ToUint();
}

bool PageNumber::operator==(const PageNumber& other) const {
  return page_number_ == other.page_number_ &&
         page_range_index_ == other.page_range_index_;
}
bool PageNumber::operator!=(const PageNumber& other) const {
  return page_number_ != other.page_number_ ||
         page_range_index_ != other.page_range_index_;
}

// static
std::vector<uint32_t> PageNumber::GetPages(PageRanges ranges,
                                           uint32_t page_count) {
  PageRange::Normalize(ranges);
  std::vector<uint32_t> printed_pages;
  static constexpr uint32_t kMaxNumberOfPages = 100000;
  printed_pages.reserve(std::min(page_count, kMaxNumberOfPages));
  for (PageNumber page_number(ranges, std::min(page_count, kMaxNumberOfPages));
       page_number != PageNumber::npos(); ++page_number) {
    printed_pages.push_back(page_number.ToUint());
  }
  return printed_pages;
}

}  // namespace printing
