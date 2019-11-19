// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_VIEW_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CORE_EXPORT DOMArrayBufferView : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  typedef ArrayBufferView::ViewType ViewType;
  static const ViewType kTypeInt8 = ArrayBufferView::kTypeInt8;
  static const ViewType kTypeUint8 = ArrayBufferView::kTypeUint8;
  static const ViewType kTypeUint8Clamped = ArrayBufferView::kTypeUint8Clamped;
  static const ViewType kTypeInt16 = ArrayBufferView::kTypeInt16;
  static const ViewType kTypeUint16 = ArrayBufferView::kTypeUint16;
  static const ViewType kTypeInt32 = ArrayBufferView::kTypeInt32;
  static const ViewType kTypeUint32 = ArrayBufferView::kTypeUint32;
  static const ViewType kTypeFloat32 = ArrayBufferView::kTypeFloat32;
  static const ViewType kTypeFloat64 = ArrayBufferView::kTypeFloat64;
  static const ViewType kTypeDataView = ArrayBufferView::kTypeDataView;

  ~DOMArrayBufferView() override = default;

  DOMArrayBuffer* buffer() const {
    DCHECK(!IsShared());
    if (!dom_array_buffer_)
      dom_array_buffer_ = DOMArrayBuffer::Create(View()->Buffer());

    return static_cast<DOMArrayBuffer*>(dom_array_buffer_.Get());
  }

  DOMSharedArrayBuffer* BufferShared() const {
    DCHECK(IsShared());
    if (!dom_array_buffer_)
      dom_array_buffer_ = DOMSharedArrayBuffer::Create(View()->Buffer());

    return static_cast<DOMSharedArrayBuffer*>(dom_array_buffer_.Get());
  }

  DOMArrayBufferBase* BufferBase() const {
    if (IsShared())
      return BufferShared();

    return buffer();
  }

  const ArrayBufferView* View() const { return buffer_view_.get(); }
  ArrayBufferView* View() { return buffer_view_.get(); }

  ViewType GetType() const { return View()->GetType(); }
  const char* TypeName() { return View()->TypeName(); }
  void* BaseAddress() const { return View()->BaseAddress(); }
  unsigned byteOffset() const { return View()->ByteOffset(); }
  unsigned byteLength() const { return View()->ByteLength(); }
  unsigned TypeSize() const { return View()->TypeSize(); }
  void SetDetachable(bool flag) { return View()->SetDetachable(flag); }
  bool IsShared() const { return View()->IsShared(); }

  void* BaseAddressMaybeShared() const {
    return View()->BaseAddressMaybeShared();
  }

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(dom_array_buffer_);
    ScriptWrappable::Trace(visitor);
  }

 protected:
  explicit DOMArrayBufferView(scoped_refptr<ArrayBufferView> buffer_view)
      : buffer_view_(std::move(buffer_view)) {
    DCHECK(buffer_view_);
  }
  DOMArrayBufferView(scoped_refptr<ArrayBufferView> buffer_view,
                     DOMArrayBufferBase* dom_array_buffer)
      : buffer_view_(std::move(buffer_view)),
        dom_array_buffer_(dom_array_buffer) {
    DCHECK(buffer_view_);
    DCHECK(dom_array_buffer_);
    DCHECK_EQ(dom_array_buffer_->Buffer(), buffer_view_->Buffer());
  }

 private:
  scoped_refptr<ArrayBufferView> buffer_view_;
  mutable Member<DOMArrayBufferBase> dom_array_buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_VIEW_H_
