// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

DOMExceptionCode RTCErrorToDOMExceptionCode(const webrtc::RTCError& error) {
  switch (error.type()) {
    case webrtc::RTCErrorType::NONE:
      // This should never happen.
      break;
    case webrtc::RTCErrorType::SYNTAX_ERROR:
      return DOMExceptionCode::kSyntaxError;
    case webrtc::RTCErrorType::INVALID_MODIFICATION:
      return DOMExceptionCode::kInvalidModificationError;
    case webrtc::RTCErrorType::NETWORK_ERROR:
      return DOMExceptionCode::kNetworkError;
    case webrtc::RTCErrorType::UNSUPPORTED_PARAMETER:
    case webrtc::RTCErrorType::UNSUPPORTED_OPERATION:
    case webrtc::RTCErrorType::RESOURCE_EXHAUSTED:
    case webrtc::RTCErrorType::INTERNAL_ERROR:
      return DOMExceptionCode::kOperationError;
    case webrtc::RTCErrorType::INVALID_STATE:
      return DOMExceptionCode::kInvalidStateError;
    case webrtc::RTCErrorType::INVALID_PARAMETER:
      // One use of this value is to signal invalid SDP syntax.
      // According to spec, this should return an RTCError with name
      // "RTCError" and detail "sdp-syntax-error", with
      // "sdpLineNumber" set to indicate the line where the error
      // occured.
      // TODO(https://crbug.com/821806): Implement the RTCError object.
      return DOMExceptionCode::kInvalidAccessError;
    case webrtc::RTCErrorType::INVALID_RANGE:
    // INVALID_RANGE should create a RangeError, which isn't a DOMException
    default:
      LOG(ERROR) << "Got unhandled RTC error "
                 << static_cast<int>(error.type());
      // No DOM equivalent.
      // Needs per-error evaluation or use ThrowExceptionFromRTCError.
      break;
  }
  NOTREACHED_NORETURN();
}

DOMException* CreateDOMExceptionFromRTCError(const webrtc::RTCError& error) {
  if (error.error_detail() != webrtc::RTCErrorDetailType::NONE &&
      (error.type() == webrtc::RTCErrorType::UNSUPPORTED_PARAMETER ||
       error.type() == webrtc::RTCErrorType::UNSUPPORTED_OPERATION ||
       error.type() == webrtc::RTCErrorType::RESOURCE_EXHAUSTED ||
       error.type() == webrtc::RTCErrorType::INTERNAL_ERROR)) {
    return MakeGarbageCollected<RTCError>(error);
  }
  return MakeGarbageCollected<DOMException>(RTCErrorToDOMExceptionCode(error),
                                            error.message());
}

void RejectPromiseFromRTCError(const webrtc::RTCError& error,
                               ScriptPromiseResolverBase* resolver) {
  if (error.type() == webrtc::RTCErrorType::INVALID_RANGE) {
    resolver->RejectWithRangeError(error.message());
    return;
  }
  resolver->RejectWithDOMException(RTCErrorToDOMExceptionCode(error),
                                   error.message());
}

void ThrowExceptionFromRTCError(const webrtc::RTCError& error,
                                ExceptionState& exception_state) {
  if (error.type() == webrtc::RTCErrorType::INVALID_RANGE) {
    exception_state.ThrowRangeError(error.message());
    return;
  }
  exception_state.ThrowDOMException(RTCErrorToDOMExceptionCode(error),
                                    error.message());
}
}  // namespace blink
