// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_page_win.h"

#include <utility>

namespace printing {

PrintedPage::PrintedPage(uint32_t page_number,
                         std::unique_ptr<MetafilePlayer> metafile,
                         const gfx::Size& page_size,
                         const gfx::Rect& page_content_rect)
    : page_number_(page_number),
      metafile_(std::move(metafile)),
      shrink_factor_(0.0f),
      page_size_(page_size),
      page_content_rect_(page_content_rect) {}

PrintedPage::~PrintedPage() = default;

const MetafilePlayer* PrintedPage::metafile() const {
  return metafile_.get();
}
}  // namespace printing
