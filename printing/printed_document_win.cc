// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "base/check.h"
#include "base/synchronization/lock.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printed_page_win.h"
#include "printing/printing_context.h"

namespace printing {

#if !defined(NDEBUG)
bool PrintedDocument::IsPageInList(const PrintedPage& page) const {
  // Make sure the page is from our list.
  base::AutoLock lock(lock_);
  return &page == mutable_.pages_.find(page.page_number() - 1)->second.get();
}
#endif

mojom::ResultCode PrintedDocument::RenderPrintedPage(
    const PrintedPage& page,
    PrintingContext* context) const {
#if !defined(NDEBUG)
  DCHECK(IsPageInList(page));
#endif

  DCHECK(context);
  mojom::ResultCode result = context->RenderPage(
      page, immutable_.settings_->page_setup_device_units());
  if (result != mojom::ResultCode::kSuccess)
    return result;

  // Beware of any asynchronous aborts of the print job that happened during
  // printing.
  if (context->PrintingAborted())
    return mojom::ResultCode::kCanceled;

  return mojom::ResultCode::kSuccess;
}

}  // namespace printing
