// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_number.h"

#include <stddef.h>

#include <limits>

#include "base/logging.h"
#include "printing/print_settings.h"

namespace printing {

PageNumber::PageNumber(const PrintSettings& settings, int document_page_count) {
  Init(settings, document_page_count);
}

PageNumber::PageNumber()
    : ranges_(NULL),
      page_number_(-1),
      page_range_index_(-1),
      document_page_count_(0) {}

void PageNumber::operator=(const PageNumber& other) {
  ranges_ = other.ranges_;
  page_number_ = other.page_number_;
  page_range_index_ = other.page_range_index_;
  document_page_count_ = other.document_page_count_;
}

void PageNumber::Init(const PrintSettings& settings, int document_page_count) {
  DCHECK(document_page_count);
  ranges_ = settings.ranges().empty() ? NULL : &settings.ranges();
  document_page_count_ = document_page_count;
  if (ranges_) {
    page_range_index_ = 0;
    page_number_ = (*ranges_)[0].from;
  } else {
    if (document_page_count) {
      page_number_ = 0;
    } else {
      page_number_ = -1;
    }
    page_range_index_ = -1;
  }
}

int PageNumber::operator++() {
  if (!ranges_) {
    // Switch to next page.
    if (++page_number_ == document_page_count_) {
      // Finished.
      *this = npos();
    }
  } else {
    // Switch to next page.
    ++page_number_;
    // Page ranges are inclusive.
    if (page_number_ > (*ranges_)[page_range_index_].to) {
      DCHECK(ranges_->size() <=
             static_cast<size_t>(std::numeric_limits<int>::max()));
      if (++page_range_index_ == static_cast<int>(ranges_->size())) {
        // Finished.
        *this = npos();
      } else {
        page_number_ = (*ranges_)[page_range_index_].from;
      }
    }
  }
  return ToInt();
}

bool PageNumber::operator==(const PageNumber& other) const {
  return page_number_ == other.page_number_ &&
         page_range_index_ == other.page_range_index_;
}
bool PageNumber::operator!=(const PageNumber& other) const {
  return page_number_ != other.page_number_ ||
         page_range_index_ != other.page_range_index_;
}

}  // namespace printing
