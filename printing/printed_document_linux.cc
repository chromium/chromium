// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/printing_context_linux.h"

#if defined(OS_ANDROID) || BUILDFLAG(IS_ASH)
#error "This file is not used on Android / ChromeOS ash-chrome"
#endif

namespace printing {

bool PrintedDocument::RenderPrintedDocument(PrintingContext* context) {
  if (context->NewPage() != PrintingContext::OK)
    return false;
  {
    base::AutoLock lock(lock_);
    const MetafilePlayer* metafile = GetMetafile();
    static_cast<PrintingContextLinux*>(context)->PrintDocument(*metafile);
  }
  return context->PageDone() == PrintingContext::OK;
}

}  // namespace printing
