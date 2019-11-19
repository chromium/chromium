// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include <ApplicationServices/ApplicationServices.h>
#include <stddef.h>
#include <stdint.h>

#include "base/mac/scoped_cftyperef.h"
#include "printing/metafile.h"
#include "ui/gfx/geometry/rect.h"

namespace printing {

bool Image::LoadMetafile(const Metafile& metafile) {
  // Load only the first page of |metafile|, just like Windows.
  const unsigned int page_number = 1;
  gfx::Rect rect(metafile.GetPageBounds(page_number));
  if (rect.width() < 1 || rect.height() < 1)
    return false;

  size_ = rect.size();
  row_length_ = size_.width() * sizeof(uint32_t);
  size_t bytes = row_length_ * size_.height();
  DCHECK(bytes);

  data_.resize(bytes);
  base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  base::ScopedCFTypeRef<CGContextRef> bitmap_context(CGBitmapContextCreate(
      &*data_.begin(), size_.width(), size_.height(), 8, row_length_,
      color_space, kCGImageAlphaPremultipliedLast));
  DCHECK(bitmap_context.get());

  struct Metafile::MacRenderPageParams params;
  params.shrink_to_fit = true;
  metafile.RenderPage(page_number, bitmap_context, rect.ToCGRect(), params);
  return true;
}

}  // namespace printing
