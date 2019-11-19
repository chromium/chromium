// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_DATA_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_DATA_VIEW_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"

namespace blink {

class CORE_EXPORT DOMDataView final : public DOMArrayBufferView {
  DEFINE_WRAPPERTYPEINFO();

 public:
  typedef char ValueType;

  static DOMDataView* Create(DOMArrayBufferBase*,
                             unsigned byte_offset,
                             unsigned byte_length);

  DOMDataView(scoped_refptr<ArrayBufferView> data_view,
              DOMArrayBufferBase* dom_array_buffer)
      : DOMArrayBufferView(std::move(data_view), dom_array_buffer) {}

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_DATA_VIEW_H_
