// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/attributed_string_type_converters.h"

#include <AppKit/AppKit.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/common_param_traits.h"

namespace mojo {

namespace {

NSDictionary* ToAttributesDictionary(std::u16string name, float font_size) {
  DCHECK(!name.empty());
  NSFont* font = [NSFont fontWithName:base::SysUTF16ToNSString(name)
                                 size:font_size];
  if (!font) {
    // This can happen if:
    // - The specified font is unavailable in this process.
    // - The specified font is a system font unavailable through `+[NSFont
    //   fontWithName:size]`.
    // In either case, the system font is a reasonable substitute.
    font = [NSFont systemFontOfSize:font_size];
  }
  return @{NSFontAttributeName : font};
}

}  // namespace

CFAttributedStringRef
TypeConverter<CFAttributedStringRef, ui::mojom::AttributedStringPtr>::Convert(
    const ui::mojom::AttributedStringPtr& mojo_attributed_string) {
  // Create the return value.
  NSString* plain_text =
      base::SysUTF16ToNSString(mojo_attributed_string->string);
  NSMutableAttributedString* decoded_string =
      [[NSMutableAttributedString alloc] initWithString:plain_text];
  // Iterate over all the encoded attributes, attaching each to the string.
  const std::vector<ui::mojom::FontAttributePtr>& attributes =
      mojo_attributed_string->attributes;
  for (const auto& attribute : attributes) {
    // Protect against ranges that are outside the range of the string.
    const gfx::Range& range = attribute.get()->effective_range;
    if (range.GetMin() > plain_text.length ||
        range.GetMax() > plain_text.length) {
      continue;
    }
    [decoded_string
        addAttributes:ToAttributesDictionary(attribute.get()->font_name,
                                             attribute.get()->font_point_size)
                range:range.ToNSRange()];
  }

  return static_cast<CFAttributedStringRef>(
      CFAutorelease(base::apple::NSToCFOwnershipCast(decoded_string)));
}

ui::mojom::AttributedStringPtr
TypeConverter<ui::mojom::AttributedStringPtr, CFAttributedStringRef>::Convert(
    const CFAttributedStringRef cf_attributed_string) {
  NSAttributedString* ns_attributed_string =
      base::apple::CFToNSPtrCast(cf_attributed_string);

  // Create the return value.
  ui::mojom::AttributedStringPtr attributed_string =
      ui::mojom::AttributedString::New();
  attributed_string->string =
      base::SysNSStringToUTF16(ns_attributed_string.string);

  // Iterate over all the attributes in the string.
  NSUInteger length = ns_attributed_string.length;
  for (NSUInteger i = 0; i < length;) {
    NSRange effective_range;
    NSDictionary* ns_attributes =
        [ns_attributed_string attributesAtIndex:i
                                 effectiveRange:&effective_range];

    NSFont* font = ns_attributes[NSFontAttributeName];
    std::u16string font_name;
    float font_point_size;
    // Only encode the attributes if the filtered set contains font information.
    if (font) {
      font_name = base::SysNSStringToUTF16(font.fontName);
      font_point_size = font.pointSize;
      if (!font_name.empty()) {
        // Convert the attributes.
        ui::mojom::FontAttributePtr attrs = ui::mojom::FontAttribute::New(
            font_name, font_point_size, gfx::Range(effective_range));
        attributed_string->attributes.push_back(std::move(attrs));
      }
    }
    // Advance the iterator to the position outside of the effective range.
    i = NSMaxRange(effective_range);
  }
  return attributed_string;
}

}  // namespace mojo
