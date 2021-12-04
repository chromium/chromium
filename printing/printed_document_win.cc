// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_number.h"
#include "printing/printed_page_win.h"
#include "printing/printing_context_win.h"
#include "printing/printing_utils.h"
#include "printing/units.h"
#include "skia/ext/skia_utils_win.h"

namespace {

// Helper class to ensure that a saved device context state gets restored at end
// of scope.
class ScopedSavedState {
 public:
  ScopedSavedState(printing::NativeDrawingContext context)
      : context_(context), saved_state_(SaveDC(context)) {
    DCHECK_NE(saved_state_, 0);
  }
  ~ScopedSavedState() {
    BOOL res = RestoreDC(context_, saved_state_);
    DCHECK_NE(res, 0);
  }

 private:
  printing::NativeDrawingContext context_;
  int saved_state_;
};

void SimpleModifyWorldTransform(HDC context,
                                int offset_x,
                                int offset_y,
                                float shrink_factor) {
  XFORM xform = {0};
  xform.eDx = static_cast<float>(offset_x);
  xform.eDy = static_cast<float>(offset_y);
  xform.eM11 = xform.eM22 = 1.f / shrink_factor;
  BOOL res = ModifyWorldTransform(context, &xform, MWT_LEFTMULTIPLY);
  DCHECK_NE(res, 0);
}

printing::mojom::ResultCode OnError() {
  return logging::GetLastSystemErrorCode() == ERROR_ACCESS_DENIED
             ? printing::mojom::ResultCode::kAccessDenied
             : printing::mojom::ResultCode::kFailed;
}

}  // namespace

namespace printing {

mojom::ResultCode PrintedDocument::RenderPrintedPage(
    const PrintedPage& page,
    printing::NativeDrawingContext context) const {
#ifndef NDEBUG
  {
    // Make sure the page is from our list.
    base::AutoLock lock(lock_);
    DCHECK(&page == mutable_.pages_.find(page.page_number() - 1)->second.get());
  }
#endif

  DCHECK(context);

  const PageSetup& page_setup = immutable_.settings_->page_setup_device_units();
  gfx::Rect content_area = GetCenteredPageContentRect(
      page_setup.physical_size(), page.page_size(), page.page_content_rect());

  // Save the state to make sure the context this function call does not modify
  // the device context.
  ScopedSavedState saved_state(context);
  skia::InitializeDC(context);
  {
    // Save the state (again) to apply the necessary world transformation.
    ScopedSavedState saved_state_inner(context);

    // Setup the matrix to translate and scale to the right place. Take in
    // account the actual shrinking factor.
    // Note that the printing output is relative to printable area of the page.
    // That is 0,0 is offset by PHYSICALOFFSETX/Y from the page.
    SimpleModifyWorldTransform(
        context, content_area.x() - page_setup.printable_area().x(),
        content_area.y() - page_setup.printable_area().y(),
        page.shrink_factor());

    if (::StartPage(context) <= 0)
      return OnError();

    bool played_back = page.metafile()->SafePlayback(context);
    DCHECK(played_back);
    if (::EndPage(context) <= 0)
      return OnError();
  }

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintedDocument::RenderPrintedDocument(
    PrintingContext* context) {
  mojom::ResultCode result = context->NewPage();
  if (result != mojom::ResultCode::kSuccess)
    return result;

  std::wstring device_name =
      base::UTF16ToWide(immutable_.settings_->device_name());
  {
    base::AutoLock lock(lock_);
    const MetafilePlayer* metafile = GetMetafile();
    static_cast<PrintingContextWin*>(context)->PrintDocument(
        device_name, *(static_cast<const MetafileSkia*>(metafile)));
  }
  return context->PageDone();
}

}  // namespace printing
