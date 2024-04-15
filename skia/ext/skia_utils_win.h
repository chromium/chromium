// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_WIN_H_
#define SKIA_EXT_SKIA_UTILS_WIN_H_

#include <windows.h>

#include <vector>

#include "base/win/scoped_gdi_object.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRefCnt.h"

struct SkIRect;
struct SkPoint;
struct SkRect;
class SkSurface;
typedef unsigned long DWORD;
typedef DWORD COLORREF;
typedef struct tagPOINT POINT;
typedef struct tagRECT RECT;

namespace skia {

// Converts a Skia point to a Windows POINT.
POINT SkPointToPOINT(const SkPoint& point);

// Converts a Windows RECT to a Skia rect.
SkRect RECTToSkRect(const RECT& rect);

// Converts a Windows RECT to a Skia rect.
// Both use same in-memory format. Verified by static_assert in
// skia_utils_win.cc.
inline const SkIRect& RECTToSkIRect(const RECT& rect) {
  return reinterpret_cast<const SkIRect&>(rect);
}

// Converts a Skia rect to a Windows RECT.
// Both use same in-memory format. Verified by static_assert in
// skia_utils_win.cc.
inline const RECT& SkIRectToRECT(const SkIRect& rect) {
  return reinterpret_cast<const RECT&>(rect);
}

// Converts COLORREFs (0BGR) to the ARGB layout Skia expects.
SK_API SkColor COLORREFToSkColor(COLORREF color);

// Converts ARGB to COLORREFs (0BGR).
SK_API COLORREF SkColorToCOLORREF(SkColor color);

// Initializes the default settings and colors in a device context.
SK_API void InitializeDC(HDC context);

// Converts scale, skew, and translation to Windows format and sets it on the
// HDC.
SK_API void LoadTransformToDC(HDC dc, const SkMatrix& matrix);

// Copies |src_rect| from source into destination.
//   Takes a potentially-slower path if |is_opaque| is false.
//   Sets |transform| on source afterwards!
SK_API void CopyHDC(HDC source, HDC destination, int x, int y, bool is_opaque,
                    const RECT& src_rect, const SkMatrix& transform);

// Creates a surface writing to the pixels backing |context|'s bitmap.
// Returns null on error.
SK_API sk_sp<SkSurface> MapPlatformSurface(HDC context);

// Creates a bitmap backed by the same pixels backing the HDC's bitmap.
// Returns an empty bitmap on error. The HDC's bitmap is assumed to be formatted
// as 32-bits-per-pixel XRGB8888, as created by CreateHBitmapXRGB8888().
SK_API SkBitmap MapPlatformBitmap(HDC context);

// Fills in a BITMAPINFOHEADER structure to hold the pixel data from |bitmap|.
// The |bitmap| must be have a color type of kN32_SkColorType, and the header
// will be for a bitmap with 32-bits-per-pixel RGB data (the high bits are
// unused in each pixel).
SK_API void CreateBitmapHeaderForN32SkBitmap(const SkBitmap& bitmap,
                                             BITMAPINFOHEADER* hdr);

// Creates a globally allocated memory containing the given byte array. The
// returned handle to the global memory is allocated by ::GlobalAlloc(), and
// must be explicitly freed by ::GlobalFree(), unless ownership is passed to the
// Win32 API. On failure, it returns null.
SK_API HGLOBAL
CreateHGlobalForByteArray(const std::vector<unsigned char>& byte_array);

// Creates an HBITMAP backed by 32-bits-per-pixel RGB data (the high bits are
// unused in each pixel), with dimensions and the RGBC pixel data from the
// SkBitmap. Any alpha channel values are copied into the HBITMAP but are not
// used. Can return a null HBITMAP on any failure to create the HBITMAP.
SK_API base::win::ScopedBitmap CreateHBitmapFromN32SkBitmap(
    const SkBitmap& bitmap);

// Creates an image in the DIBV5 format. On success this function returns a
// handle to an allocated memory block containing a DIBV5 header followed by the
// pixel data. If the bitmap creation fails, it returns null. This is preferred
// in some cases over the HBITMAP format because it handles transparency better.
// The returned handle to the global memory is allocated by ::GlobalAlloc(), and
// must be explicitly freed by ::GlobalFree(), unless ownership is passed to the
// Win32 API.
SK_API HGLOBAL CreateDIBV5ImageDataFromN32SkBitmap(const SkBitmap& bitmap);

// Fills in a BITMAPINFOHEADER structure given the bitmap's size. The header
// will be for a bitmap with 32-bits-per-pixel RGB data (the high bits are
// unused in each pixel).
SK_API void CreateBitmapHeaderForXRGB888(int width,
                                         int height,
                                         BITMAPINFOHEADER* hdr);

// Creates an HBITMAP backed by 32-bits-per-pixel RGB data (the high bits are
// unused in each pixel).
SK_API base::win::ScopedBitmap CreateHBitmapXRGB8888(
    int width,
    int height,
    HANDLE shared_section = nullptr,
    void** data = nullptr);

}  // namespace skia

#endif  // SKIA_EXT_SKIA_UTILS_WIN_H_

