// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REGISTRY_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CustomElementDefinitionBuilder;
class CustomElementDescriptor;
class CustomElementReactionStack;
class Element;
class ElementDefinitionOptions;
class ExceptionState;
class LocalDOMWindow;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;
class V0CustomElementRegistrationContext;
class V8CustomElementConstructor;

class CORE_EXPORT CustomElementRegistry final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CustomElementRegistry(const LocalDOMWindow*);
  ~CustomElementRegistry() override = default;

  CustomElementDefinition* define(ScriptState*,
                                  const AtomicString& name,
                                  V8CustomElementConstructor* constructor,
                                  const ElementDefinitionOptions*,
                                  ExceptionState&);

  ScriptValue get(const AtomicString& name);
  bool NameIsDefined(const AtomicString& name) const;
  CustomElementDefinition* DefinitionForName(const AtomicString& name) const;
  CustomElementDefinition* DefinitionForId(CustomElementDefinition::Id) const;

  // TODO(dominicc): Switch most callers of definitionForName to
  // definitionFor when implementing type extensions.
  CustomElementDefinition* DefinitionFor(const CustomElementDescriptor&) const;

  // TODO(dominicc): Consider broadening this API when type extensions are
  // implemented.
  void AddCandidate(Element&);
  ScriptPromise whenDefined(ScriptState*,
                            const AtomicString& name,
                            ExceptionState&);
  void upgrade(Node* root);

  void Entangle(V0CustomElementRegistrationContext*);

  void Trace(Visitor*) override;

 private:
  CustomElementDefinition* DefineInternal(ScriptState*,
                                          const AtomicString& name,
                                          CustomElementDefinitionBuilder&,
                                          const ElementDefinitionOptions*,
                                          ExceptionState&);

  bool V0NameIsDefined(const AtomicString& name);

  void CollectCandidates(const CustomElementDescriptor&,
                         HeapVector<Member<Element>>*);

  bool element_definition_is_running_;

  using DefinitionList = HeapVector<Member<CustomElementDefinition>>;
  DefinitionList definitions_;

  using NameIdMap = HashMap<AtomicString, CustomElementDefinition::Id>;
  NameIdMap name_id_map_;

  Member<const LocalDOMWindow> owner_;

  using V0RegistrySet =
      HeapHashSet<WeakMember<V0CustomElementRegistrationContext>>;
  Member<V0RegistrySet> v0_;

  using UpgradeCandidateSet = HeapHashSet<WeakMember<Element>>;
  using UpgradeCandidateMap =
      HeapHashMap<AtomicString, Member<UpgradeCandidateSet>>;
  Member<UpgradeCandidateMap> upgrade_candidates_;

  using WhenDefinedPromiseMap =
      HeapHashMap<AtomicString, Member<ScriptPromiseResolver>>;
  WhenDefinedPromiseMap when_defined_promise_map_;

  Member<CustomElementReactionStack> reaction_stack_;

  FRIEND_TEST_ALL_PREFIXES(
      CustomElementTest,
      CreateElement_TagNameCaseHandlingCreatingCustomElement);
  friend class CustomElementRegistryTest;

  DISALLOW_COPY_AND_ASSIGN(CustomElementRegistry);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REGISTRY_H_
