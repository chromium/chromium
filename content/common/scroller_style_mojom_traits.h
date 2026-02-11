// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SCROLLER_STYLE_MOJOM_TRAITS_H_
#define CONTENT_COMMON_SCROLLER_STYLE_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"

namespace mojo {

template <>
struct EnumTraits<content::mojom::ScrollerStyle, blink::ScrollerStyle> {
  static content::mojom::ScrollerStyle ToMojom(blink::ScrollerStyle in) {
    switch (in) {
      case blink::ScrollerStyle::kScrollerStyleLegacy:
        return content::mojom::ScrollerStyle::kScrollerStyleLegacy;
      case blink::ScrollerStyle::kScrollerStyleOverlay:
        return content::mojom::ScrollerStyle::kScrollerStyleOverlay;
    }
    NOTREACHED();
  }

  static blink::ScrollerStyle FromMojom(content::mojom::ScrollerStyle in) {
    switch (in) {
      case content::mojom::ScrollerStyle::kScrollerStyleLegacy:
        return blink::ScrollerStyle::kScrollerStyleLegacy;
      case content::mojom::ScrollerStyle::kScrollerStyleOverlay:
        return blink::ScrollerStyle::kScrollerStyleOverlay;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_SCROLLER_STYLE_MOJOM_TRAITS_H_
