// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_H_

#include "base/allocator/partition_allocator/oom.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT DOMArrayBuffer final : public DOMArrayBufferBase {
  DEFINE_WRAPPERTYPEINFO();
  static const WrapperTypeInfo wrapper_type_info_body_;

 public:
  static DOMArrayBuffer* Create(ArrayBufferContents contents) {
    return MakeGarbageCollected<DOMArrayBuffer>(std::move(contents));
  }
  static DOMArrayBuffer* Create(size_t num_elements, size_t element_byte_size) {
    ArrayBufferContents contents(num_elements, element_byte_size,
                                 ArrayBufferContents::kNotShared,
                                 ArrayBufferContents::kZeroInitialize);
    if (UNLIKELY(!contents.Data())) {
      OOM_CRASH(num_elements * element_byte_size);
    }
    return Create(std::move(contents));
  }
  static DOMArrayBuffer* Create(const void* source, size_t byte_length) {
    ArrayBufferContents contents(byte_length, 1,
                                 ArrayBufferContents::kNotShared,
                                 ArrayBufferContents::kDontInitialize);
    if (UNLIKELY(!contents.Data())) {
      OOM_CRASH(byte_length);
    }
    memcpy(contents.Data(), source, byte_length);
    return Create(std::move(contents));
  }

  static DOMArrayBuffer* Create(scoped_refptr<SharedBuffer>);
  static DOMArrayBuffer* Create(const Vector<base::span<const char>>&);

  static DOMArrayBuffer* CreateOrNull(size_t num_elements,
                                      size_t element_byte_size);

  // Only for use by XMLHttpRequest::responseArrayBuffer,
  // Internals::serializeObject, and
  // FetchDataLoaderAsArrayBuffer::OnStateChange.
  static DOMArrayBuffer* CreateUninitializedOrNull(size_t num_elements,
                                                   size_t element_byte_size);

  explicit DOMArrayBuffer(ArrayBufferContents contents)
      : DOMArrayBufferBase(std::move(contents)) {}

  DOMArrayBuffer* Slice(size_t begin, size_t end) const;

  bool IsDetachable(v8::Isolate*);

  // Transfer the ArrayBuffer if it is detachable, otherwise make a copy and
  // transfer that.
  bool Transfer(v8::Isolate*, ArrayBufferContents& result);

  // Share the ArrayBuffer, even if it is non-shared. Such sharing is necessary
  // for e.g. WebAudio which uses a separate thread for processing the
  // ArrayBuffer while at the same time exposing a NonShared Float32Array.
  bool ShareNonSharedForInternalUse(ArrayBufferContents& result) {
    if (!Content()->BackingStore()) {
      result.Detach();
      return false;
    }
    Content()->ShareNonSharedForInternalUse(result);
    return true;
  }

  v8::MaybeLocal<v8::Value> Wrap(ScriptState*) override;

 private:
  bool TransferDetachable(v8::Isolate*, ArrayBufferContents& result);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_H_
