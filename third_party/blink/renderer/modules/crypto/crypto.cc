/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/crypto/crypto.h"

#include "crypto/random.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

bool IsIntegerArray(NotShared<DOMArrayBufferView> array) {
  DOMArrayBufferView::ViewType type = array->GetType();
  return type == DOMArrayBufferView::kTypeInt8 ||
         type == DOMArrayBufferView::kTypeUint8 ||
         type == DOMArrayBufferView::kTypeUint8Clamped ||
         type == DOMArrayBufferView::kTypeInt16 ||
         type == DOMArrayBufferView::kTypeUint16 ||
         type == DOMArrayBufferView::kTypeInt32 ||
         type == DOMArrayBufferView::kTypeUint32 ||
         type == DOMArrayBufferView::kTypeBigInt64 ||
         type == DOMArrayBufferView::kTypeBigUint64;
}

}  // namespace

NotShared<DOMArrayBufferView> Crypto::getRandomValues(
    NotShared<DOMArrayBufferView> array,
    ExceptionState& exception_state) {
  DCHECK(array);
  if (!IsIntegerArray(array)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTypeMismatchError,
        String::Format("The provided ArrayBufferView is of type '%s', which is "
                       "not an integer array type.",
                       array->TypeName()));
    return NotShared<DOMArrayBufferView>(nullptr);
  }
  if (array->byteLength() > 65536) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kQuotaExceededError,
        String::Format("The ArrayBufferView's byte length (%zu) exceeds the "
                       "number of bytes of entropy available via this API "
                       "(65536).",
                       array->byteLength()));
    return NotShared<DOMArrayBufferView>(nullptr);
  }
  crypto::RandBytes(array->ByteSpan());
  return array;
}

String Crypto::randomUUID() {
  return WTF::CreateCanonicalUUIDString();
}

SubtleCrypto* Crypto::subtle() {
  if (!subtle_crypto_)
    subtle_crypto_ = MakeGarbageCollected<SubtleCrypto>();
  return subtle_crypto_.Get();
}

void Crypto::Trace(Visitor* visitor) const {
  visitor->Trace(subtle_crypto_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
