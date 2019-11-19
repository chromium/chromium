// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_SHARED_ARRAY_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_SHARED_ARRAY_BUFFER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"

namespace blink {

class CORE_EXPORT DOMSharedArrayBuffer final : public DOMArrayBufferBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMSharedArrayBuffer* Create(scoped_refptr<ArrayBuffer> buffer) {
    DCHECK(buffer->IsShared());
    return MakeGarbageCollected<DOMSharedArrayBuffer>(std::move(buffer));
  }
  static DOMSharedArrayBuffer* Create(unsigned num_elements,
                                      unsigned element_byte_size) {
    return Create(ArrayBuffer::CreateShared(num_elements, element_byte_size));
  }
  static DOMSharedArrayBuffer* Create(const void* source,
                                      unsigned byte_length) {
    return Create(ArrayBuffer::CreateShared(source, byte_length));
  }
  static DOMSharedArrayBuffer* Create(ArrayBufferContents& contents) {
    DCHECK(contents.IsShared());
    return Create(ArrayBuffer::Create(contents));
  }

  explicit DOMSharedArrayBuffer(scoped_refptr<ArrayBuffer> buffer)
      : DOMArrayBufferBase(std::move(buffer)) {}

  bool ShareContentsWith(ArrayBufferContents& result) {
    return Buffer()->ShareContentsWith(result);
  }

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_SHARED_ARRAY_BUFFER_H_
