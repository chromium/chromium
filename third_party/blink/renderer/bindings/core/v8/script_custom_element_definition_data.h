// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_DATA_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CustomElementRegistry;
class ScriptState;
class V8CustomElementAdoptedCallback;
class V8CustomElementAttributeChangedCallback;
class V8CustomElementConstructor;
class V8CustomElementFormAssociatedCallback;
class V8CustomElementFormDisabledCallback;
class V8CustomElementFormStateRestoreCallback;
class V8VoidFunction;

class ScriptCustomElementDefinitionData {
  STACK_ALLOCATED();

 public:
  ScriptCustomElementDefinitionData() {}

  Member<ScriptState> script_state_;
  Member<CustomElementRegistry> registry_;
  Member<V8CustomElementConstructor> constructor_;
  Member<V8VoidFunction> connected_callback_;
  Member<V8VoidFunction> disconnected_callback_;
  Member<V8CustomElementAdoptedCallback> adopted_callback_;
  Member<V8CustomElementAttributeChangedCallback> attribute_changed_callback_;
  Member<V8CustomElementFormAssociatedCallback> form_associated_callback_;
  Member<V8VoidFunction> form_reset_callback_;
  Member<V8CustomElementFormDisabledCallback> form_disabled_callback_;
  Member<V8CustomElementFormStateRestoreCallback> form_state_restore_callback_;
  HashSet<AtomicString> observed_attributes_;
  Vector<String> disabled_features_;
  bool is_form_associated_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScriptCustomElementDefinitionData);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_DATA_H_
