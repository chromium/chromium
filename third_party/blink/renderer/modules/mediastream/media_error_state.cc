/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/mediastream/media_error_state.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

MediaErrorState::MediaErrorState()
    : error_type_(kNoError), code_(DOMExceptionCode::kNoError) {}

void MediaErrorState::ThrowTypeError(const String& message) {
  error_type_ = kTypeError;
  message_ = message;
}

void MediaErrorState::ThrowDOMException(DOMExceptionCode code,
                                        const String& message) {
  error_type_ = kDOMException;
  code_ = code;
  message_ = message;
}

void MediaErrorState::ThrowConstraintError(const String& message,
                                           const String& constraint) {
  error_type_ = kConstraintError;
  message_ = message;
  constraint_ = constraint;
}

void MediaErrorState::Reset() {
  error_type_ = kNoError;
}

bool MediaErrorState::HadException() {
  return error_type_ != kNoError;
}

bool MediaErrorState::CanGenerateException() {
  return error_type_ == kTypeError || error_type_ == kDOMException;
}

void MediaErrorState::RaiseException(ExceptionState& target) {
  switch (error_type_) {
    case kNoError:
      NOTREACHED();
      break;
    case kTypeError:
      target.ThrowTypeError(message_);
      break;
    case kDOMException:
      target.ThrowDOMException(code_, message_);
      break;
    case kConstraintError:
      // This is for the cases where we can't pass back a
      // NavigatorUserMediaError.
      // So far, we have this in the constructor of RTCPeerConnection,
      // which is due to be deprecated.
      // TODO(hta): Remove this code. https://crbug.com/576581
      target.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                               "Unsatisfiable constraint " + constraint_);
      break;
    default:
      NOTREACHED();
  }
}

String MediaErrorState::GetErrorMessage() {
  switch (error_type_) {
    case kNoError:
      NOTREACHED();
      break;
    case kTypeError:
    case kDOMException:
      return message_;
    case kConstraintError:
      // This is for the cases where we can't pass back a
      // NavigatorUserMediaError.
      // So far, we have this in the constructor of RTCPeerConnection,
      // which is due to be deprecated.
      // TODO(hta): Remove this code. https://crbug.com/576581
      return "Unsatisfiable constraint " + constraint_;
    default:
      NOTREACHED();
  }

  return String();
}

V8UnionDOMExceptionOrOverconstrainedError* MediaErrorState::CreateError() {
  DCHECK_EQ(error_type_, kConstraintError);
  return MakeGarbageCollected<V8UnionDOMExceptionOrOverconstrainedError>(
      MakeGarbageCollected<OverconstrainedError>(constraint_, message_));
}

}  // namespace blink
