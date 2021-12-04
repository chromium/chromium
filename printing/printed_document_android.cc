// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "printing/mojom/print.mojom.h"
#include "printing/printing_context_android.h"

namespace printing {

mojom::ResultCode PrintedDocument::RenderPrintedDocument(
    PrintingContext* context) {
  mojom::ResultCode result = context->NewPage();
  if (result != mojom::ResultCode::kSuccess)
    return result;
  {
    base::AutoLock lock(lock_);
    const MetafilePlayer* metafile = GetMetafile();
    static_cast<PrintingContextAndroid*>(context)->PrintDocument(*metafile);
  }
  return context->PageDone();
}

}  // namespace printing
