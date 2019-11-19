/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_V0_CUSTOM_ELEMENT_LIFECYCLE_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_V0_CUSTOM_ELEMENT_LIFECYCLE_CALLBACKS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_lifecycle_callbacks.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "v8/include/v8.h"

namespace blink {

class V0CustomElementBinding;
class V0CustomElementLifecycleCallbacks;
class Element;
class V8PerContextData;

class V8V0CustomElementLifecycleCallbacks final
    : public V0CustomElementLifecycleCallbacks {
 public:
  V8V0CustomElementLifecycleCallbacks(
      ScriptState*,
      v8::Local<v8::Object> prototype,
      v8::MaybeLocal<v8::Function> created,
      v8::MaybeLocal<v8::Function> attached,
      v8::MaybeLocal<v8::Function> detached,
      v8::MaybeLocal<v8::Function> attribute_changed);
  ~V8V0CustomElementLifecycleCallbacks() override;

  bool SetBinding(std::unique_ptr<V0CustomElementBinding>);

  void Trace(blink::Visitor*) override;

 private:
  void Created(Element*) override;
  void Attached(Element*) override;
  void Detached(Element*) override;
  void AttributeChanged(Element*,
                        const AtomicString& name,
                        const AtomicString& old_value,
                        const AtomicString& new_value) override;

  void Call(const TraceWrapperV8Reference<v8::Function>& callback_reference,
            Element*);

  V8PerContextData* CreationContextData();

  Member<ScriptState> script_state_;
  TraceWrapperV8Reference<v8::Object> prototype_;
  TraceWrapperV8Reference<v8::Function> created_;
  TraceWrapperV8Reference<v8::Function> attached_;
  TraceWrapperV8Reference<v8::Function> detached_;
  TraceWrapperV8Reference<v8::Function> attribute_changed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_V0_CUSTOM_ELEMENT_LIFECYCLE_CALLBACKS_H_
