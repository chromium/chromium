// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"

namespace blink {

// static
bool ThrowIfCodecStateClosed(V8CodecState state,
                             String operation,
                             ExceptionState& exception_state) {
  if (state.AsEnum() != V8CodecState::Enum::kClosed)
    return false;

  exception_state.ThrowDOMException(
      DOMExceptionCode::kInvalidStateError,
      "Cannot call '" + operation + "' on a closed codec.");
  return true;
}

// static
bool ThrowIfCodecStateUnconfigured(V8CodecState state,
                                   String operation,
                                   ExceptionState& exception_state) {
  if (state.AsEnum() != V8CodecState::Enum::kUnconfigured)
    return false;

  exception_state.ThrowDOMException(
      DOMExceptionCode::kInvalidStateError,
      "Cannot call '" + operation + "' on an unconfigured codec.");
  return true;
}

}  // namespace blink
