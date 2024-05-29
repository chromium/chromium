// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_PUBLIC_MOJOM_ACCESSIBILITY_HELPER_MOJOM_TRAITS_H_
#define SERVICES_ACCESSIBILITY_ANDROID_PUBLIC_MOJOM_ACCESSIBILITY_HELPER_MOJOM_TRAITS_H_

#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {

template <>
struct StructTraits<ax::android::mojom::RectDataView, gfx::Rect> {
  static int32_t left(const gfx::Rect& r) { return r.x(); }
  static int32_t top(const gfx::Rect& r) { return r.y(); }
  static int32_t right(const gfx::Rect& r) { return r.right(); }
  static int32_t bottom(const gfx::Rect& r) { return r.bottom(); }

  static bool Read(ax::android::mojom::RectDataView data, gfx::Rect* out);
};

}  // namespace mojo

#endif  // SERVICES_ACCESSIBILITY_ANDROID_PUBLIC_MOJOM_ACCESSIBILITY_HELPER_MOJOM_TRAITS_H_
