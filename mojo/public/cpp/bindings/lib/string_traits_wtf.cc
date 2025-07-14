// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/string_traits_wtf.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/string_data_view.h"

namespace mojo {

// static
void StringTraits<blink::String>::SetToNull(blink::String* output) {
  if (output->IsNull())
    return;

  blink::String result;
  output->swap(result);
}

// static
blink::StringUtf8Adaptor StringTraits<blink::String>::GetUTF8(
    const blink::String& input) {
  return blink::StringUtf8Adaptor(input);
}

// static
bool StringTraits<blink::String>::Read(StringDataView input,
                                       blink::String* output) {
  blink::String result = blink::String::FromUTF8(input.value());
  output->swap(result);
  return true;
}

}  // namespace mojo
