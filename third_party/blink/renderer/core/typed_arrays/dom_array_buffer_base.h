// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_BASE_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CORE_EXPORT DOMArrayBufferBase : public ScriptWrappable {
 public:
  ~DOMArrayBufferBase() override = default;

  const ArrayBufferContents* Content() const { return &contents_; }
  ArrayBufferContents* Content() { return &contents_; }

  const void* Data() const { return contents_.Data(); }
  void* Data() { return contents_.Data(); }

  const void* DataMaybeShared() const { return contents_.DataMaybeShared(); }
  void* DataMaybeShared() { return contents_.DataMaybeShared(); }

  size_t ByteLength() const { return contents_.DataLength(); }

  bool IsDetached() const { return is_detached_; }

  void Detach() { is_detached_ = true; }

  bool IsShared() const { return contents_.IsShared(); }

  // ScriptWrappable overrides:
  v8::MaybeLocal<v8::Value> Wrap(ScriptState*) override {
    NOTREACHED();
    return v8::Local<v8::Value>();
  }

 protected:
  explicit DOMArrayBufferBase(ArrayBufferContents contents)
      : contents_(std::move(contents)) {}

  ArrayBufferContents contents_;
  bool is_detached_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_BASE_H_
