// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PAGE_NUMBER_H_
#define PRINTING_PAGE_NUMBER_H_

#include <ostream>
#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "printing/page_range.h"

namespace printing {

// Represents a page series using the array of page ranges. Pages are assumed
// to be 0-indexed.
class COMPONENT_EXPORT(PRINTING) PageNumber {
 public:
  // Initializes the page to the first page in the ranges or 0.
  PageNumber(const PageRanges& ranges, uint32_t document_page_count);

  PageNumber();

  PageNumber(const PageNumber& other);
  PageNumber& operator=(const PageNumber& other);

  // Initializes the page to the first page in the ranges or 0.
  // Initializes to npos if the ranges is empty and document_page_count is 0.
  void Init(const PageRanges& ranges, uint32_t document_page_count);

  // Converts to a page numbers.
  uint32_t ToUint() const {
    DCHECK(*this == npos() || page_number_ < document_page_count_);
    return page_number_;
  }

  // Calculates the next page in the series. Sets this PageNumber to
  // PageNumber::npos() if we reach document_page_count_.
  uint32_t operator++();

  // Returns an instance that represents the end of a series.
  static const PageNumber npos() { return PageNumber(); }

  // Equality operator. Only the current page number is verified so that
  // "page != PageNumber::npos()" works.
  bool operator==(const PageNumber& other) const;
  bool operator!=(const PageNumber& other) const;

  // Returns all pages represented by the given PageRanges up to and including
  // page document_page_count - 1.
  static std::vector<uint32_t> GetPages(PageRanges ranges,
                                        uint32_t document_page_count);

 private:
  // The page range to follow.
  raw_ptr<const PageRanges> ranges_;

  // The next page to be printed. `kInvalidPageIndex` when not printing.
  uint32_t page_number_;

  // The next page to be printed. `kInvalidPageIndex` when not used. Valid only
  // if document()->settings().range.empty() is false.
  uint32_t page_range_index_;

  // Total number of pages in the underlying document, including outside of the
  // specified ranges.
  uint32_t document_page_count_;
};

// Debug output support.
template <class E, class T>
inline typename std::basic_ostream<E, T>& operator<<(
    typename std::basic_ostream<E, T>& ss,
    const PageNumber& page) {
  return ss << page.ToUint();
}

}  // namespace printing

#endif  // PRINTING_PAGE_NUMBER_H_
