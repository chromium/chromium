// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <psapi.h>
#include <stddef.h>
#include <string.h>

#include "base/debug/gdi_debug_util_win.h"
#include "base/memory/ptr_util.h"
#include "base/logging.h"
#include "base/win/win_util.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkRect.h"

namespace {

struct HDCContextRec {
  HDC hdc_;
  HBITMAP prev_bitmap_;
};

static void DeleteHDCCallback(void*, void* context) {
  DCHECK_NE(context, nullptr);
  const HDCContextRec* rec = static_cast<const HDCContextRec*>(context);

  // Must select back in the old bitmap before we delete the hdc, and so we can
  // recover the new_bitmap that we allocated, so we can delete it.
  HBITMAP new_bitmap =
      static_cast<HBITMAP>(SelectObject(rec->hdc_, rec->prev_bitmap_));
  bool success = DeleteObject(new_bitmap);
  DCHECK(success);
  success = DeleteDC(rec->hdc_);
  DCHECK(success);
  delete rec;
}

// Allocate the layer and fill in the fields for the Rec, or return false
// on error.
static bool Create(int width,
                   int height,
                   bool is_opaque,
                   HANDLE shared_section,
                   bool do_clear,
                   SkRasterHandleAllocator::Rec* rec) {
  void* pixels;
  HBITMAP new_bitmap =
      skia::CreateHBitmap(width, height, is_opaque, shared_section, &pixels);
  if (!new_bitmap) {
    LOG(ERROR) << "CreateHBitmap failed";
    return false;
  }

  size_t row_bytes = skia::PlatformCanvasStrideForWidth(width);
  if (do_clear)
    memset(pixels, 0, row_bytes * height);

  HDC hdc = CreateCompatibleDC(nullptr);
  if (!hdc) {
    DeleteObject(new_bitmap);
    return false;
  }
  SetGraphicsMode(hdc, GM_ADVANCED);

  HBITMAP prev_bitmap = static_cast<HBITMAP>(SelectObject(hdc, new_bitmap));
  DCHECK(prev_bitmap);

  rec->fReleaseProc = DeleteHDCCallback;
  rec->fReleaseCtx = new HDCContextRec{hdc, prev_bitmap};
  rec->fPixels = pixels;
  rec->fRowBytes = row_bytes;
  rec->fHandle = hdc;
  return true;
}

/**
 *  Subclass of SkRasterHandleAllocator that returns an HDC as its "handle".
 */
class GDIAllocator : public SkRasterHandleAllocator {
 public:
  GDIAllocator() {}

  bool allocHandle(const SkImageInfo& info, Rec* rec) override {
    SkASSERT(info.colorType() == kN32_SkColorType);
    return Create(info.width(), info.height(), info.isOpaque(), nullptr,
                  !info.isOpaque(), rec);
  }

  void updateHandle(Handle handle,
                    const SkMatrix& ctm,
                    const SkIRect& clip_bounds) override {
    HDC hdc = static_cast<HDC>(handle);
    skia::LoadTransformToDC(hdc, ctm);

    HRGN hrgn = CreateRectRgnIndirect(&skia::SkIRectToRECT(clip_bounds));
    int result = SelectClipRgn(hdc, hrgn);
    DCHECK(result != ERROR);
    result = DeleteObject(hrgn);
    DCHECK(result != 0);
  }
};

void unmap_view_proc(void* pixels, void*) {
  UnmapViewOfFile(pixels);
}

}  // namespace

namespace skia {

std::unique_ptr<SkCanvas> CreatePlatformCanvasWithSharedSection(
    int width,
    int height,
    bool is_opaque,
    HANDLE shared_section,
    OnFailureType failure_type) {
  SkAlphaType alpha = is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
  SkImageInfo info = SkImageInfo::MakeN32(width, height, alpha);
  size_t row_bytes = PlatformCanvasStrideForWidth(width);

  // This function contains an implementation of a Skia platform bitmap for
  // drawing and compositing graphics. The original implementation uses Windows
  // GDI to create the backing bitmap memory, however it's possible for a
  // process to not have access to GDI which will cause this code to fail. It's
  // possible to detect when GDI is unavailable and instead directly map the
  // shared memory as the bitmap.
  if (base::win::IsUser32AndGdi32Available()) {
    SkRasterHandleAllocator::Rec rec;
    if (Create(width, height, is_opaque, shared_section, false, &rec))
      return SkRasterHandleAllocator::MakeCanvas(
          std::make_unique<GDIAllocator>(), info, &rec);
  } else {
    DCHECK(shared_section != NULL);
    void* pixels =
        MapViewOfFile(shared_section, FILE_MAP_WRITE, 0, 0, row_bytes * height);
    if (pixels) {
      SkBitmap bitmap;
      if (bitmap.installPixels(info, pixels, row_bytes, unmap_view_proc,
                               nullptr)) {
        return std::make_unique<SkCanvas>(bitmap);
      }
    }
  }

  CHECK(failure_type != CRASH_ON_FAILURE);
  return nullptr;
}

HDC GetNativeDrawingContext(SkCanvas* canvas) {
  return canvas ? static_cast<HDC>(canvas->accessTopRasterHandle()) : nullptr;
}

}  // namespace skia
