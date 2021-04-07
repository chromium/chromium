// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_UTILS_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"

namespace blink {

// Extracts the read/write operation size from the buffer size.
int NativeIOOperationSize(const DOMArrayBufferView& buffer);

// Transfers the buffer from source to a new DOMArrayBufferView, preserving the
// its specific type. Returns nullptr when source is not detachable or when the
// transfer fails.
DOMArrayBufferView* TransferToNewArrayBufferView(
    v8::Isolate* isolate,
    NotShared<DOMArrayBufferView> source);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_UTILS_H_
