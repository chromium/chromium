// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PAGE_NUMBER_H_
#define PRINTING_PAGE_NUMBER_H_

#include <ostream>

#include "printing/page_range.h"

namespace printing {

class PrintSettings;

// Represents a page series following the array of page ranges defined in a
// PrintSettings.
class PRINTING_EXPORT PageNumber {
 public:
  // Initializes the page to the first page in the settings's range or 0.
  PageNumber(const PrintSettings& settings, int document_page_count);

  PageNumber();

  void operator=(const PageNumber& other);

  // Initializes the page to the first page in the setting's range or 0. It
  // initialize to npos if the range is empty and document_page_count is 0.
  void Init(const PrintSettings& settings, int document_page_count);

  // Converts to a page numbers.
  int ToInt() const { return page_number_; }

  // Calculates the next page in the serie.
  int operator++();

  // Returns an instance that represents the end of a serie.
  static const PageNumber npos() { return PageNumber(); }

  // Equality operator. Only the current page number is verified so that
  // "page != PageNumber::npos()" works.
  bool operator==(const PageNumber& other) const;
  bool operator!=(const PageNumber& other) const;

 private:
  // The page range to follow.
  const PageRanges* ranges_;

  // The next page to be printed. -1 when not printing.
  int page_number_;

  // The next page to be printed. -1 when not used. Valid only if
  // document()->settings().range.empty() is false.
  int page_range_index_;

  // Number of expected pages in the document. Used when ranges_ is NULL.
  int document_page_count_;
};

// Debug output support.
template <class E, class T>
inline typename std::basic_ostream<E, T>& operator<<(
    typename std::basic_ostream<E, T>& ss,
    const PageNumber& page) {
  return ss << page.ToInt();
}

}  // namespace printing

#endif  // PRINTING_PAGE_NUMBER_H_
