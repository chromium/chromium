// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_BIG_STRING_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_BIG_STRING_MOJOM_TRAITS_H_

#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/big_string.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT StructTraits<mojo_base::mojom::BigStringDataView,
                                    blink::String> {
  static bool IsNull(const blink::String& input) { return input.IsNull(); }
  static void SetToNull(blink::String* output) { *output = blink::String(); }

  static mojo_base::BigBuffer data(const blink::String& input) {
    blink::StringUtf8Adaptor adaptor(input);
    return mojo_base::BigBuffer(base::as_byte_span(adaptor));
  }

  static bool Read(mojo_base::mojom::BigStringDataView data,
                   blink::String* out) {
    mojo_base::BigBuffer buffer;
    if (!data.ReadData(&buffer)) {
      return false;
    }
    // An empty |mojo_base::BigBuffer| may have a null |data()| if empty.
    if (!buffer.size()) {
      *out = blink::g_empty_string;
    } else {
      *out = blink::String::FromUTF8(base::span(buffer));
    }
    return true;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_BIG_STRING_MOJOM_TRAITS_H_
