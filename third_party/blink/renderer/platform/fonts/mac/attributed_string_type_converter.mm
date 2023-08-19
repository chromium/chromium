// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/mac/attributed_string_type_converter.h"

#include <AppKit/AppKit.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/range/range.h"

namespace mojo {

ui::mojom::blink::AttributedStringPtr
TypeConverter<ui::mojom::blink::AttributedStringPtr, CFAttributedStringRef>::
    Convert(CFAttributedStringRef cf_attributed_string) {
  NSAttributedString* ns_attributed_string =
      base::apple::CFToNSPtrCast(cf_attributed_string);

  // Create the return value.
  ui::mojom::blink::AttributedStringPtr attributed_string =
      ui::mojom::blink::AttributedString::New();
  attributed_string->string = String(ns_attributed_string.string);

  // Iterate over all the attributes in the string.
  NSUInteger length = ns_attributed_string.length;
  for (NSUInteger i = 0; i < length;) {
    NSRange effective_range;
    NSDictionary* ns_attributes =
        [ns_attributed_string attributesAtIndex:i
                                 effectiveRange:&effective_range];

    NSFont* font = ns_attributes[NSFontAttributeName];
    String font_name;
    float font_point_size;
    // Only encode the attributes if the filtered set contains font information.
    if (font) {
      font_name = String(font.fontName);
      font_point_size = font.pointSize;
      if (!font_name.empty()) {
        // Convert the attributes.
        ui::mojom::blink::FontAttributePtr attrs =
            ui::mojom::blink::FontAttribute::New(font_name, font_point_size,
                                                 gfx::Range(effective_range));
        attributed_string->attributes.push_back(std::move(attrs));
      }
    }
    // Advance the iterator to the position outside of the effective range.
    i = NSMaxRange(effective_range);
  }
  return attributed_string;
}

}  // namespace mojo
