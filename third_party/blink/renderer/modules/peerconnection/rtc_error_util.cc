// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

DOMException* CreateDOMExceptionFromRTCError(const webrtc::RTCError& error) {
  switch (error.type()) {
    case webrtc::RTCErrorType::NONE:
      // This should never happen.
      NOTREACHED();
      break;
    case webrtc::RTCErrorType::SYNTAX_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                                error.message());
    case webrtc::RTCErrorType::INVALID_MODIFICATION:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidModificationError, error.message());
    case webrtc::RTCErrorType::NETWORK_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kNetworkError,
                                                error.message());
    case webrtc::RTCErrorType::UNSUPPORTED_PARAMETER:
    case webrtc::RTCErrorType::UNSUPPORTED_OPERATION:
    case webrtc::RTCErrorType::RESOURCE_EXHAUSTED:
    case webrtc::RTCErrorType::INTERNAL_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, error.message());
    case webrtc::RTCErrorType::INVALID_STATE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, error.message());
    case webrtc::RTCErrorType::INVALID_PARAMETER:
      // One use of this value is to signal invalid SDP syntax.
      // According to spec, this should return an RTCError with name
      // "RTCError" and detail "sdp-syntax-error", with
      // "sdpLineNumber" set to indicate the line where the error
      // occured.
      // TODO(https://crbug.com/821806): Implement the RTCError object.
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidAccessError, error.message());
    case webrtc::RTCErrorType::INVALID_RANGE:
    // INVALID_RANGE should create a RangeError, which isn't a DOMException
    default:
      LOG(ERROR) << "Got unhandled RTC error "
                 << static_cast<int>(error.type());
      // No DOM equivalent.
      // Needs per-error evaluation or use ThrowExceptionFromRTCError.
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}

void ThrowExceptionFromRTCError(const webrtc::RTCError& error,
                                ExceptionState& exception_state) {
  switch (error.type()) {
    case webrtc::RTCErrorType::NONE:
      // This should never happen.
      NOTREACHED();
      break;
    case webrtc::RTCErrorType::SYNTAX_ERROR:
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        error.message());
      return;
    case webrtc::RTCErrorType::INVALID_MODIFICATION:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError, error.message());
      return;
    case webrtc::RTCErrorType::NETWORK_ERROR:
      exception_state.ThrowDOMException(DOMExceptionCode::kNetworkError,
                                        error.message());
      return;
    case webrtc::RTCErrorType::UNSUPPORTED_PARAMETER:
    case webrtc::RTCErrorType::UNSUPPORTED_OPERATION:
    case webrtc::RTCErrorType::RESOURCE_EXHAUSTED:
    case webrtc::RTCErrorType::INTERNAL_ERROR:
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        error.message());
      return;
    case webrtc::RTCErrorType::INVALID_STATE:
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        error.message());
      return;
    case webrtc::RTCErrorType::INVALID_PARAMETER:
      // One use of this value is to signal invalid SDP syntax.
      // According to spec, this should return an RTCError with name
      // "RTCError" and detail "sdp-syntax-error", with
      // "sdpLineNumber" set to indicate the line where the error
      // occured.
      // TODO(https://crbug.com/821806): Implement the RTCError object.
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                        error.message());
      return;
    case webrtc::RTCErrorType::INVALID_RANGE:
      exception_state.ThrowRangeError(error.message());
      return;
    default:
      LOG(ERROR) << "Got unhandled RTC error "
                 << static_cast<int>(error.type());
      NOTREACHED();
      break;
  }
  NOTREACHED();
}
}  // namespace blink
