/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_REGISTRATION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_REGISTRATION_CONTEXT_H_

#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_upgrade_candidate_map.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CustomElementRegistry;

class V0CustomElementRegistrationContext final
    : public GarbageCollected<V0CustomElementRegistrationContext> {
 public:
  V0CustomElementRegistrationContext();
  ~V0CustomElementRegistrationContext() = default;
  void DocumentWasDetached() { registry_.DocumentWasDetached(); }

  // Definitions
  void RegisterElement(Document*,
                       V0CustomElementConstructorBuilder*,
                       const AtomicString& type,
                       ExceptionState&);

  Element* CreateCustomTagElement(Document&, const QualifiedName&);
  static void SetIsAttributeAndTypeExtension(Element*,
                                             const AtomicString& type);
  static void SetTypeExtension(Element*, const AtomicString& type);

  void Resolve(Element*, const V0CustomElementDescriptor&);

  bool NameIsDefined(const AtomicString& name) const;
  void SetV1(const CustomElementRegistry*);

  void Trace(Visitor*);

  // Instance creation
  void DidGiveTypeExtension(Element*, const AtomicString& type);

 private:
  void ResolveOrScheduleResolution(Element*,
                                   const AtomicString& type_extension);

  V0CustomElementRegistry registry_;

  // Element creation
  Member<V0CustomElementUpgradeCandidateMap> candidates_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_REGISTRATION_CONTEXT_H_
