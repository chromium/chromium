// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include "base/logging.h"
#include "printing/metafile.h"
#include "printing/printing_context.h"

namespace printing {

bool PrintedDocument::RenderPrintedDocument(PrintingContext* context) {
  DCHECK(context);

  const MetafilePlayer* metafile;
  gfx::Size page_size;
  gfx::Rect page_content_rect;
  {
    base::AutoLock lock(lock_);
    metafile = GetMetafile();
    page_size = mutable_.page_size_;
    page_content_rect = mutable_.page_content_rect_;
  }

  DCHECK(metafile);
  const PageSetup& page_setup = immutable_.settings_->page_setup_device_units();
  gfx::Rect content_area = GetCenteredPageContentRect(
      page_setup.physical_size(), page_size, page_content_rect);

  struct Metafile::MacRenderPageParams params;
  params.autorotate = true;
  size_t num_pages = expected_page_count();
  for (size_t metafile_page_number = 1; metafile_page_number <= num_pages;
       metafile_page_number++) {
    if (context->NewPage() != PrintingContext::OK)
      return false;
    metafile->RenderPage(metafile_page_number, context->context(),
                         content_area.ToCGRect(), params);
    if (context->PageDone() != PrintingContext::OK)
      return false;
  }
  return true;
}

}  // namespace printing
