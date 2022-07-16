// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "printing/mojom/print.mojom.h"
#include "printing/printing_context_android.h"

namespace printing {

bool PrintedDocument::RenderPrintedDocument(PrintingContext* context) {
  if (context->NewPage() != mojom::ResultCode::kSuccess)
    return false;
  {
    base::AutoLock lock(lock_);
    const MetafilePlayer* metafile = GetMetafile();
    static_cast<PrintingContextAndroid*>(context)->PrintDocument(*metafile);
  }
  return context->PageDone() == mojom::ResultCode::kSuccess;
}

}  // namespace printing
