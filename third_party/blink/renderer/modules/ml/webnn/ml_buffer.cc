// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"

#include "third_party/blink/renderer/modules/ml/ml_context.h"

namespace blink {

MLBuffer::MLBuffer(MLContext* context, uint64_t size)
    : ml_context_(context), size_(size) {}

MLBuffer::~MLBuffer() = default;

void MLBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  ScriptWrappable::Trace(visitor);
}

void MLBuffer::destroy() {
  DestroyImpl();
}

uint64_t MLBuffer::size() const {
  return size_;
}

}  // namespace blink
