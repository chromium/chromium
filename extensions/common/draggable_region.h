// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_DRAGGABLE_REGION_H_
#define EXTENSIONS_COMMON_DRAGGABLE_REGION_H_

#include "ui/gfx/geometry/rect.h"

namespace extensions {

struct DraggableRegion {
  bool draggable;
  gfx::Rect bounds;

  DraggableRegion();
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_DRAGGABLE_REGION_H_
