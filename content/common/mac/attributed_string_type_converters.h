// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_ATTRIBUTED_STRING_TYPE_CONVERTERS_H_
#define CONTENT_COMMON_MAC_ATTRIBUTED_STRING_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "ui/base/mojom/attributed_string.mojom.h"
#include "ui/gfx/range/range.h"

#include <CoreFoundation/CoreFoundation.h>

namespace mojo {

template <>
struct CONTENT_EXPORT
    TypeConverter<CFAttributedStringRef, ui::mojom::AttributedStringPtr> {
  static CFAttributedStringRef Convert(
      const ui::mojom::AttributedStringPtr& mojo_attributed_string);
};

template <>
struct CONTENT_EXPORT
    TypeConverter<ui::mojom::AttributedStringPtr, CFAttributedStringRef> {
  static ui::mojom::AttributedStringPtr Convert(
      const CFAttributedStringRef cf_attributed_string);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_MAC_ATTRIBUTED_STRING_TYPE_CONVERTERS_H_
