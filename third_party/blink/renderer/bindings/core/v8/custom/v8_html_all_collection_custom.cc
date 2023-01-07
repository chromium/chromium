/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "third_party/blink/renderer/bindings/core/v8/v8_html_all_collection.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_htmlcollection.h"
#include "third_party/blink/renderer/core/html/html_all_collection.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"

namespace blink {

// https://html.spec.whatwg.org/C/#the-htmlallcollection-interface
//
// The only part of the spec expressed in terms of ECMAScript values instead of
// IDL values is the [[Call]] internal method. However, the way the
// |item(nameOrIndex)| method is defined makes it indistinguishable. This
// implementation does not match step-for-step the definition or [[Call]] or
// |item(nameOrIndex)|, but should produce the same result.

void GetIndexedOrNamed(const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() == 0 || info[0]->IsUndefined()) {
    V8SetReturnValueNull(info);
    return;
  }

  HTMLAllCollection* impl = V8HTMLAllCollection::ToImpl(info.Holder());

  v8::Local<v8::Uint32> index;
  if (info[0]
          ->ToArrayIndex(info.GetIsolate()->GetCurrentContext())
          .ToLocal(&index)) {
    Element* result = impl->AnonymousIndexedGetter(index->Value());
    bindings::V8SetReturnValue(info, result, impl);
    return;
  }

  TOSTRING_VOID(V8StringResource<>, name, info[0]);
  ScriptState* script_state =
      ScriptState::From(info.This()->GetCreationContextChecked());
  v8::Local<v8::Value> v8_value;
  if (!ToV8Traits<IDLNullable<V8UnionElementOrHTMLCollection>>::ToV8(
           script_state, impl->NamedGetter(name))
           .ToLocal(&v8_value)) {
    return;
  }
  bindings::V8SetReturnValue(info, v8_value);
}

void V8HTMLAllCollection::LegacyCallCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      info.GetIsolate(), "Blink_V8HTMLAllCollection_legacyCallCustom");
  GetIndexedOrNamed(info);
}

void V8HTMLAllCollection::ItemMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      info.GetIsolate(), "Blink_V8HTMLAllCollection_itemMethodCustom");
  GetIndexedOrNamed(info);
}

}  // namespace blink
