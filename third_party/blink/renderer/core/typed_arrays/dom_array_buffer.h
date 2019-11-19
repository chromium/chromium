// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT DOMArrayBuffer final : public DOMArrayBufferBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMArrayBuffer* Create(scoped_refptr<ArrayBuffer> buffer) {
    return MakeGarbageCollected<DOMArrayBuffer>(std::move(buffer));
  }
  static DOMArrayBuffer* Create(size_t num_elements, size_t element_byte_size) {
    return Create(ArrayBuffer::Create(num_elements, element_byte_size));
  }
  static DOMArrayBuffer* Create(const void* source, size_t byte_length) {
    return Create(ArrayBuffer::Create(source, byte_length));
  }
  static DOMArrayBuffer* Create(ArrayBufferContents& contents) {
    return Create(ArrayBuffer::Create(contents));
  }
  static DOMArrayBuffer* Create(scoped_refptr<SharedBuffer>);
  static DOMArrayBuffer* Create(const Vector<base::span<const char>>&);

  // Only for use by XMLHttpRequest::responseArrayBuffer,
  // Internals::serializeObject, and
  // FetchDataLoaderAsArrayBuffer::OnStateChange.
  static DOMArrayBuffer* CreateUninitializedOrNull(size_t num_elements,
                                                   size_t element_byte_size);

  explicit DOMArrayBuffer(scoped_refptr<ArrayBuffer> buffer)
      : DOMArrayBufferBase(std::move(buffer)) {}

  DOMArrayBuffer* Slice(unsigned begin, unsigned end) const {
    return Create(Buffer()->Slice(begin, end));
  }

  bool IsDetachable(v8::Isolate*);

  // Transfer the ArrayBuffer if it is detachable, otherwise make a copy and
  // transfer that.
  bool Transfer(v8::Isolate*, ArrayBufferContents& result);

  // Share the ArrayBuffer, even if it is non-shared. Such sharing is necessary
  // for e.g. WebAudio which uses a separate thread for processing the
  // ArrayBuffer while at the same time exposing a NonShared Float32Array.
  bool ShareNonSharedForInternalUse(ArrayBufferContents& result) {
    return Buffer()->ShareNonSharedForInternalUse(result);
  }

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_H_
