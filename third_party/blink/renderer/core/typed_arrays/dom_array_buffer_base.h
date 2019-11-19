// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CORE_EXPORT DOMArrayBufferBase : public ScriptWrappable {
 public:
  ~DOMArrayBufferBase() override = default;

  const ArrayBuffer* Buffer() const { return buffer_.get(); }
  ArrayBuffer* Buffer() { return buffer_.get(); }

  const void* Data() const { return Buffer()->Data(); }
  void* Data() { return Buffer()->Data(); }

  size_t ByteLengthAsSizeT() const { return Buffer()->ByteLengthAsSizeT(); }

  // This function is deprecated and should not be used. Use {ByteLengthAsSizeT}
  // instead.
  unsigned DeprecatedByteLengthAsUnsigned() const {
    size_t size = ByteLengthAsSizeT();
    CHECK_LE(size, static_cast<size_t>(std::numeric_limits<unsigned>::max()));
    return static_cast<unsigned>(size);
  }

  bool IsDetached() const { return Buffer()->IsDetached(); }
  bool IsShared() const { return Buffer()->IsShared(); }

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

 protected:
  explicit DOMArrayBufferBase(scoped_refptr<ArrayBuffer> buffer)
      : buffer_(std::move(buffer)) {
    DCHECK(buffer_);
  }

  scoped_refptr<ArrayBuffer> buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_BASE_H_
