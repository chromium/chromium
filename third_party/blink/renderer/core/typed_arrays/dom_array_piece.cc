// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"

namespace blink {

DOMArrayPiece::DOMArrayPiece(
    const ArrayBufferOrArrayBufferView& array_buffer_or_view,
    InitWithUnionOption option) {
  if (array_buffer_or_view.IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer = array_buffer_or_view.GetAsArrayBuffer();
    InitWithArrayBuffer(array_buffer->Buffer());
  } else if (array_buffer_or_view.IsArrayBufferView()) {
    DOMArrayBufferView* array_buffer_view =
        array_buffer_or_view.GetAsArrayBufferView().View();
    InitWithArrayBufferView(array_buffer_view->View());
  } else if (array_buffer_or_view.IsNull() &&
             option == kAllowNullPointToNullWithZeroSize) {
    InitWithData(nullptr, 0);
  }  // Otherwise, leave the obejct as null.
}

}  // namespace blink
