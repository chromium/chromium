/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_REGISTRY_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_descriptor_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CustomElementRegistry;
class ExceptionState;
class V0CustomElementConstructorBuilder;

class V0CustomElementRegistry final {
  DISALLOW_NEW();

 public:
  void Trace(blink::Visitor*);
  void DocumentWasDetached() { document_was_detached_ = true; }

 protected:
  friend class V0CustomElementRegistrationContext;

  V0CustomElementRegistry() : document_was_detached_(false) {}

  V0CustomElementDefinition* RegisterElement(
      Document*,
      V0CustomElementConstructorBuilder*,
      const AtomicString& name,
      ExceptionState&);
  V0CustomElementDefinition* Find(const V0CustomElementDescriptor&) const;

  bool NameIsDefined(const AtomicString& name) const;
  void SetV1(const CustomElementRegistry*);

 private:
  bool V1NameIsDefined(const AtomicString& name) const;

  typedef HeapHashMap<V0CustomElementDescriptor,
                      Member<V0CustomElementDefinition>>
      DefinitionMap;
  DefinitionMap definitions_;
  HashSet<AtomicString> registered_type_names_;
  Member<const CustomElementRegistry> v1_;
  bool document_was_detached_;

  DISALLOW_COPY_AND_ASSIGN(V0CustomElementRegistry);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_REGISTRY_H_
