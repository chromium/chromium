// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_WTF_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_WTF_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/string_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) StringTraits<blink::String> {
  static bool IsNull(const blink::String& input) { return input.IsNull(); }
  static void SetToNull(blink::String* output);

  static blink::StringUtf8Adaptor GetUTF8(const blink::String& input);

  static bool Read(StringDataView input, blink::String* output);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_WTF_H_
