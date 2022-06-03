// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include "base/check.h"
#include "printing/metafile.h"
#include "printing/printing_context.h"

namespace printing {

bool PrintedDocument::RenderPrintedDocument(PrintingContext* context) {
  DCHECK(context);

  const MetafilePlayer* metafile;
  {
    base::AutoLock lock(lock_);
    metafile = GetMetafile();
  }

  DCHECK(metafile);
  const PageSetup& page_setup = immutable_.settings_->page_setup_device_units();
  const CGRect paper_rect = gfx::Rect(page_setup.physical_size()).ToCGRect();

  size_t num_pages = expected_page_count();
  for (size_t metafile_page_number = 1; metafile_page_number <= num_pages;
       metafile_page_number++) {
    if (context->NewPage() != mojom::ResultCode::kSuccess)
      return false;
    metafile->RenderPage(metafile_page_number, context->context(), paper_rect,
                         /*autorotate=*/true, /*fit_to_page=*/false);
    if (context->PageDone() != mojom::ResultCode::kSuccess)
      return false;
  }
  return true;
}

}  // namespace printing
