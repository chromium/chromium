// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTED_PAGE_WIN_H_
#define PRINTING_PRINTED_PAGE_WIN_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "printing/metafile.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

// Contains the data to reproduce a printed page, either on screen or on
// paper. Once created, this object is immutable. It has no reference to the
// PrintedDocument containing this page.
// Note: May be accessed from many threads at the same time. This is an non
// issue since this object is immutable. The reason is that a page may be
// printed and be displayed at the same time.
class COMPONENT_EXPORT(PRINTING) PrintedPage
    : public base::RefCountedThreadSafe<PrintedPage> {
 public:
  PrintedPage(uint32_t page_number,
              std::unique_ptr<MetafilePlayer> metafile,
              const gfx::Size& page_size,
              const gfx::Rect& page_content_rect);
  PrintedPage(const PrintedPage&) = delete;
  PrintedPage& operator=(const PrintedPage&) = delete;

  // Getters
  uint32_t page_number() const { return page_number_; }
  const MetafilePlayer* metafile() const;
  const gfx::Size& page_size() const { return page_size_; }
  const gfx::Rect& page_content_rect() const { return page_content_rect_; }
  void set_shrink_factor(float shrink_factor) {
    shrink_factor_ = shrink_factor;
  }
  float shrink_factor() const { return shrink_factor_; }

 private:
  friend class base::RefCountedThreadSafe<PrintedPage>;

  ~PrintedPage();

  // Page number inside the printed document.
  const uint32_t page_number_;

  // Actual paint data.
  const std::unique_ptr<MetafilePlayer> metafile_;

  // Shrink done in comparison to desired_dpi.
  float shrink_factor_;

  // The physical page size. To support multiple page formats inside on print
  // job.
  const gfx::Size page_size_;

  // The printable area of the page.
  const gfx::Rect page_content_rect_;
};

}  // namespace printing

#endif  // PRINTING_PRINTED_PAGE_WIN_H_
