// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/string_traits_wtf.h"

#include "mojo/public/cpp/bindings/string_data_view.h"

namespace mojo {

// static
void StringTraits<WTF::String>::SetToNull(WTF::String* output) {
  if (output->IsNull())
    return;

  WTF::String result;
  output->swap(result);
}

// static
WTF::StringUTF8Adaptor StringTraits<WTF::String>::GetUTF8(
    const WTF::String& input) {
  return WTF::StringUTF8Adaptor(input);
}

// static
bool StringTraits<WTF::String>::Read(StringDataView input,
                                     WTF::String* output) {
  WTF::String result = WTF::String::FromUTF8(input.storage(), input.size());
  output->swap(result);
  return true;
}

}  // namespace mojo
