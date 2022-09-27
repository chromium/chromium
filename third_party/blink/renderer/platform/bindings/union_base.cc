// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/union_base.h"

#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace bindings {

// static
void UnionBase::ThrowTypeErrorNotOfType(ExceptionState& exception_state,
                                        const char* expected_type) {
  exception_state.ThrowTypeError(
      ExceptionMessages::ValueNotOfType(expected_type));
}

}  // namespace bindings

}  // namespace blink
