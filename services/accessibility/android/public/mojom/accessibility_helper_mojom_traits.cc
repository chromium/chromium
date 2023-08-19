// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/public/mojom/accessibility_helper_mojom_traits.h"

namespace mojo {

bool StructTraits<ax::android::mojom::RectDataView, gfx::Rect>::Read(
    ax::android::mojom::RectDataView data,
    gfx::Rect* out) {
  if (data.right() < data.left() || data.bottom() < data.top()) {
    return false;
  }

  out->SetRect(data.left(), data.top(), data.right() - data.left(),
               data.bottom() - data.top());
  return true;
}

}  // namespace mojo
