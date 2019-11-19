/*
* Copyright (C) 2009 Google Inc. All rights reserved.
* Copyright (C) 2014 Opera Software ASA. All rights reserved.
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

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_embed_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_object_element.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

template <typename ElementType>
void GetScriptableObjectProperty(
    const AtomicString& name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ScriptState* state = ScriptState::Current(info.GetIsolate());
  if (!state->World().IsMainWorld()) {
    if (state->World().IsIsolatedWorld()) {
      UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                        WebFeature::kPluginInstanceAccessFromIsolatedWorld);
    }
    // The plugin system cannot deal with multiple worlds, so block any
    // non-main world access.
    return;
  }
  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                    WebFeature::kPluginInstanceAccessFromMainWorld);

  HTMLPlugInElement* impl = ElementType::ToImpl(info.Holder());
  v8::Local<v8::Object> instance = impl->PluginWrapper();
  if (instance.IsEmpty())
    return;

  v8::Local<v8::String> v8_name = V8AtomicString(info.GetIsolate(), name);
  bool has_own_property;
  v8::Local<v8::Value> value;
  if (!instance->HasOwnProperty(state->GetContext(), v8_name)
           .To(&has_own_property) ||
      !has_own_property ||
      !instance->Get(state->GetContext(), v8_name).ToLocal(&value)) {
    return;
  }

  V8SetReturnValue(info, value);
}

template <typename ElementType>
void SetScriptableObjectProperty(
    const AtomicString& name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DCHECK(!value.IsEmpty());
  ScriptState* state = ScriptState::Current(info.GetIsolate());
  if (!state->World().IsMainWorld()) {
    // The plugin system cannot deal with multiple worlds, so block any
    // non-main world access.
    return;
  }

  HTMLPlugInElement* impl = ElementType::ToImpl(info.Holder());
  v8::Local<v8::Object> instance = impl->PluginWrapper();
  if (instance.IsEmpty())
    return;

  // Don't intercept any of the properties of the HTMLPluginElement.
  v8::Local<v8::String> v8_name = V8AtomicString(info.GetIsolate(), name);
  v8::Local<v8::Context> context = state->GetContext();
  bool instance_has_property;
  bool holder_has_property;
  if (!instance->HasOwnProperty(context, v8_name).To(&instance_has_property) ||
      !info.Holder()->Has(context, v8_name).To(&holder_has_property) ||
      (!instance_has_property && holder_has_property)) {
    return;
  }

  // FIXME: The gTalk pepper plugin is the only plugin to make use of
  // SetProperty and that is being deprecated. This can be removed as soon as
  // it goes away.
  // Call SetProperty on a pepper plugin's scriptable object. Note that we
  // never set the return value here which would indicate that the plugin has
  // intercepted the SetProperty call, which means that the property on the
  // DOM element will also be set. For plugin's that don't intercept the call
  // (all except gTalk) this makes no difference at all. For gTalk the fact
  // that the property on the DOM element also gets set is inconsequential.
  bool created;
  if (!instance->CreateDataProperty(context, v8_name, value).To(&created))
    return;

  V8SetReturnValue(info, value);
}

}  // namespace

void V8HTMLEmbedElement::NamedPropertyGetterCustom(
    const AtomicString& name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                    WebFeature::kHTMLEmbedElementGetter);
  GetScriptableObjectProperty<V8HTMLEmbedElement>(name, info);
}

void V8HTMLObjectElement::NamedPropertyGetterCustom(
    const AtomicString& name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                    WebFeature::kHTMLObjectElementGetter);
  GetScriptableObjectProperty<V8HTMLObjectElement>(name, info);
}

void V8HTMLEmbedElement::NamedPropertySetterCustom(
    const AtomicString& name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                    WebFeature::kHTMLEmbedElementSetter);
  SetScriptableObjectProperty<V8HTMLEmbedElement>(name, value, info);
}

void V8HTMLObjectElement::NamedPropertySetterCustom(
    const AtomicString& name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                    WebFeature::kHTMLObjectElementSetter);
  SetScriptableObjectProperty<V8HTMLObjectElement>(name, value, info);
}

}  // namespace blink
