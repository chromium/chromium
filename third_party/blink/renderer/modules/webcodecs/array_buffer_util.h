// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ARRAY_BUFFER_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ARRAY_BUFFER_UTIL_H_

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/types/to_address.h"
#include "media/base/decoder_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybufferallowshared_arraybufferviewallowshared.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_util.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using AllowSharedBufferSource =
    V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared;

// Ensures that the underlying memory for `buffer_union` remains valid
// (owned by a returned instance of ArrayBufferContents)
// even if the buffer is detached or truncated by client activity.
// Returns a valid ArrayBufferContents only for shared buffers, otherwise
// returns an empty ArrayBufferContents.
ArrayBufferContents PinSharedArrayBufferContent(
    const AllowSharedBufferSource* buffer_union);

// Validates `transfer_list`, then detaches all array buffers in it. Returns the
// bytes of `data` along with the ArrayBufferContents that own them: if `data`
// is backed by a buffer from `transfer_list`, the contents are valid and the
// span points into them; otherwise the contents are invalid and the span points
// into the live buffer. Throws a DataCloneError (detaching nothing) if
// `transfer_list` contains non-detachable or duplicate buffers.
std::pair<ArrayBufferContents, base::span<uint8_t>> TransferArrayBuffers(
    v8::Isolate* isolate,
    const HeapVector<Member<DOMArrayBuffer>>& transfer_list,
    DOMArrayBufferView* data,
    ExceptionState& exception_state);

// A wrapper around `ArrayBuffer` contents that allows to transfer ownership
// to `DecoderBuffer`.
class ArrayBufferContentsExternalMemory
    : public media::DecoderBuffer::ExternalMemory {
 public:
  explicit ArrayBufferContentsExternalMemory(ArrayBufferContents contents,
                                             base::span<const uint8_t> span)
      : contents_(std::move(contents)), span_(span) {
    // Check that `span` refers to the memory inside `contents`.
    auto* contents_data = static_cast<const uint8_t*>(contents_.Data());
    CHECK_GE(base::to_address(span.begin()), contents_data);
    CHECK_LE(base::to_address(span.end()),
             UNSAFE_TODO(contents_data + contents_.DataLength()));
  }

  const base::span<const uint8_t> Span() const override { return span_; }

 private:
  ArrayBufferContents contents_;
  // TODO(367764863) Rewrite to base::raw_span.
  RAW_PTR_EXCLUSION const base::span<const uint8_t> span_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ARRAY_BUFFER_UTIL_H_
