/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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

#include "third_party/blink/renderer/bindings/core/v8/array_value.h"

#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"

namespace blink {

ArrayValue& ArrayValue::operator=(const ArrayValue& other) = default;

bool ArrayValue::IsUndefinedOrNull() const {
  return blink::IsUndefinedOrNull(array_);
}

bool ArrayValue::length(uint32_t& length) const {
  if (IsUndefinedOrNull())
    return false;

  length = array_->Length();
  return true;
}

bool ArrayValue::Get(uint32_t index, Dictionary& value) const {
  if (IsUndefinedOrNull())
    return false;

  if (index >= array_->Length())
    return false;

  DCHECK(isolate_);
  DCHECK(isolate_->IsCurrent());
  v8::Local<v8::Value> indexed_value;
  if (!array_->Get(isolate_->GetCurrentContext(), index)
           .ToLocal(&indexed_value) ||
      !indexed_value->IsObject())
    return false;

  // TODO(bashi,yukishiino): Should rethrow the exception.
  // http://crbug.com/666661
  DummyExceptionStateForTesting exception_state;
  value = Dictionary(isolate_, indexed_value, exception_state);
  return true;
}

}  // namespace blink
