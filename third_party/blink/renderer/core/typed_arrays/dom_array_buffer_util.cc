// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_util.h"

#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

std::pair<ArrayBufferContents, base::span<uint8_t>> TransferArrayBufferView(
    v8::Isolate* isolate,
    DOMArrayBufferView* view,
    ExceptionState& exception_state) {
  CHECK(!view->IsShared());
  // Capture these before transferring, as the view is detached afterwards.
  const size_t byte_offset = view->byteOffset();
  const size_t byte_length = view->byteLength();
  ArrayBufferContents contents;
  if (!view->buffer()->Transfer(isolate, contents, exception_state)) {
    return {};
  }
  base::span<uint8_t> span =
      contents.ByteSpan().subspan(byte_offset, byte_length);
  return {std::move(contents), span};
}

}  // namespace blink
