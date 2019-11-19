// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "printing/metafile.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/gfx/gdi_util.h"  // EMF support
#include "ui/gfx/geometry/rect.h"

namespace printing {

bool Image::LoadMetafile(const Metafile& metafile) {
  gfx::Rect rect(metafile.GetPageBounds(1));

  // Create a temporary HDC and bitmap to retrieve the rendered data.
  base::win::ScopedCreateDC hdc(::CreateCompatibleDC(NULL));
  BITMAPV4HEADER hdr;
  DCHECK_EQ(rect.x(), 0);
  DCHECK_EQ(rect.y(), 0);
  DCHECK_GE(rect.width(), 0);  // Metafile could be empty.
  DCHECK_GE(rect.height(), 0);

  if (rect.width() < 1 || rect.height() < 1)
    return false;

  size_ = rect.size();
  gfx::CreateBitmapV4Header(rect.width(), rect.height(), &hdr);
  unsigned char* bits = NULL;
  base::win::ScopedBitmap bitmap(
      ::CreateDIBSection(hdc.Get(), reinterpret_cast<BITMAPINFO*>(&hdr), 0,
                         reinterpret_cast<void**>(&bits), NULL, 0));
  DCHECK(bitmap.is_valid());
  base::win::ScopedSelectObject select_object(hdc.Get(), bitmap.get());

  skia::InitializeDC(hdc.Get());

  bool success = metafile.Playback(hdc.Get(), NULL);

  row_length_ = size_.width() * sizeof(uint32_t);
  size_t bytes = row_length_ * size_.height();
  DCHECK(bytes);

  data_.assign(bits, bits + bytes);

  return success;
}

}  // namespace printing
