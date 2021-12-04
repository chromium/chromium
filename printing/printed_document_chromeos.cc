// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "base/logging.h"
#include "printing/mojom/print.mojom.h"

#if defined(USE_CUPS)
#include "printing/metafile.h"
#include "printing/printing_context_chromeos.h"
#endif

namespace printing {

mojom::ResultCode PrintedDocument::RenderPrintedDocument(
    PrintingContext* context) {
#if defined(USE_CUPS)
  DCHECK(context);

  mojom::ResultCode result = context->NewPage();
  if (result != mojom::ResultCode::kSuccess)
    return result;
  {
    base::AutoLock lock(lock_);
    std::vector<char> buffer;
    const MetafilePlayer* metafile = GetMetafile();
    DCHECK(metafile);
    if (metafile->GetDataAsVector(&buffer)) {
      result =
          static_cast<PrintingContextChromeos*>(context)->StreamData(buffer);
      if (result != mojom::ResultCode::kSuccess)
        return result;
    } else {
      LOG(WARNING) << "Failed to read data from metafile";
    }
  }
  return context->PageDone();
#else
  NOTREACHED();
  return mojom::ResultCode::kFailed;
#endif  // defined(USE_CUPS)
}

}  // namespace printing
