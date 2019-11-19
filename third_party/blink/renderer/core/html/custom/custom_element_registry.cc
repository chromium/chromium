// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"

#include <limits>

#include "base/auto_reset.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "third_party/blink/renderer/bindings/core/v8/script_custom_element_definition_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_definition_options.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition_builder.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_upgrade_sorter.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace {

void CollectUpgradeCandidateInNode(Node& root,
                                   HeapVector<Member<Element>>& candidates) {
  if (auto* root_element = DynamicTo<Element>(root)) {
    if (root_element->GetCustomElementState() == CustomElementState::kUndefined)
      candidates.push_back(root_element);
    if (auto* shadow_root = root_element->GetShadowRoot()) {
      if (shadow_root->GetType() != ShadowRootType::kUserAgent)
        CollectUpgradeCandidateInNode(*shadow_root, candidates);
    }
  }
  for (auto& element : Traversal<HTMLElement>::ChildrenOf(root))
    CollectUpgradeCandidateInNode(element, candidates);
}

// Returns true if |name| is invalid.
bool ThrowIfInvalidName(const AtomicString& name,
                        bool allow_embedder_names,
                        ExceptionState& exception_state) {
  if (CustomElement::IsValidName(name, allow_embedder_names))
    return false;
  exception_state.ThrowDOMException(
      DOMExceptionCode::kSyntaxError,
      "\"" + name + "\" is not a valid custom element name");
  return true;
}

// Returns true if |name| is valid.
bool ThrowIfValidName(const AtomicString& name,
                      ExceptionState& exception_state) {
  if (!CustomElement::IsValidName(name, false))
    return false;
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      "\"" + name + "\" is a valid custom element name");
  return true;
}

}  // namespace

CustomElementRegistry::CustomElementRegistry(const LocalDOMWindow* owner)
    : element_definition_is_running_(false),
      owner_(owner),
      v0_(MakeGarbageCollected<V0RegistrySet>()),
      upgrade_candidates_(MakeGarbageCollected<UpgradeCandidateMap>()),
      reaction_stack_(&CustomElementReactionStack::Current()) {
  Document* document = owner->document();
  if (V0CustomElementRegistrationContext* v0 =
          document ? document->RegistrationContext() : nullptr)
    Entangle(v0);
}

void CustomElementRegistry::Trace(Visitor* visitor) {
  visitor->Trace(definitions_);
  visitor->Trace(owner_);
  visitor->Trace(v0_);
  visitor->Trace(upgrade_candidates_);
  visitor->Trace(when_defined_promise_map_);
  visitor->Trace(reaction_stack_);
  ScriptWrappable::Trace(visitor);
}

CustomElementDefinition* CustomElementRegistry::define(
    ScriptState* script_state,
    const AtomicString& name,
    V8CustomElementConstructor* constructor,
    const ElementDefinitionOptions* options,
    ExceptionState& exception_state) {
  ScriptCustomElementDefinitionBuilder builder(script_state, this, constructor,
                                               exception_state);
  return DefineInternal(script_state, name, builder, options, exception_state);
}

// http://w3c.github.io/webcomponents/spec/custom/#dfn-element-definition
CustomElementDefinition* CustomElementRegistry::DefineInternal(
    ScriptState* script_state,
    const AtomicString& name,
    CustomElementDefinitionBuilder& builder,
    const ElementDefinitionOptions* options,
    ExceptionState& exception_state) {
  TRACE_EVENT1("blink", "CustomElementRegistry::define", "name", name.Utf8());
  if (!builder.CheckConstructorIntrinsics())
    return nullptr;

  const bool allow_embedder_names =
      WebCustomElement::EmbedderNamesAllowedScope::IsAllowed();
  if (ThrowIfInvalidName(name, allow_embedder_names, exception_state))
    return nullptr;

  if (NameIsDefined(name) || V0NameIsDefined(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "the name \"" + name + "\" has already been used with this registry");
    return nullptr;
  }

  if (!builder.CheckConstructorNotRegistered())
    return nullptr;

  // Polymer V2/V3 uses Custom Elements V1. <dom-module> is defined in its base
  // library and is a strong signal that this is a Polymer V2+.
  if (name == "dom-module") {
    if (Document* document = owner_->document())
      UseCounter::Count(*document, WebFeature::kPolymerV2Detected);
  }
  AtomicString local_name = name;

  // Step 7. customized built-in elements definition
  // element interface extends option checks
  if (options->hasExtends()) {
    // 7.1. If element interface is valid custom element name, throw exception
    const AtomicString& extends = AtomicString(options->extends());
    if (ThrowIfValidName(AtomicString(options->extends()), exception_state))
      return nullptr;
    // 7.2. If element interface is undefined element, throw exception
    if (htmlElementTypeForTag(extends) ==
        HTMLElementType::kHTMLUnknownElement) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "\"" + extends + "\" is an HTMLUnknownElement");
      return nullptr;
    }
    // 7.3. Set localName to extends
    local_name = extends;
  }

  // 8. If this CustomElementRegistry's element definition is
  // running flag is set, then throw a "NotSupportedError"
  // DOMException and abort these steps.
  if (element_definition_is_running_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "an element definition is already being processed");
    return nullptr;
  }

  {
    // 9. Set this CustomElementRegistry's element definition is
    // running flag.
    base::AutoReset<bool> defining(&element_definition_is_running_, true);

    // 10. Run the following substeps while catching any exceptions: ...
    if (!builder.RememberOriginalProperties())
      return nullptr;

    // "Then, perform the following substep, regardless of whether
    // the above steps threw an exception or not: Unset this
    // CustomElementRegistry's element definition is running
    // flag."
    // (|defining|'s destructor does this.)
  }

  // During step 10, property getters might have detached the frame. Abort in
  // the case.
  if (!script_state->ContextIsValid()) {
    // Intentionally do not throw an exception so that, when Blink will support
    // detached frames, the behavioral change whether Blink throws or not will
    // not be observable from author.
    // TODO(yukishiino): Support detached frames.
    return nullptr;
  }

  CustomElementDescriptor descriptor(name, local_name);
  if (UNLIKELY(definitions_.size() >=
               std::numeric_limits<CustomElementDefinition::Id>::max()))
    return nullptr;
  CustomElementDefinition::Id id = definitions_.size() + 1;
  CustomElementDefinition* definition = builder.Build(descriptor, id);
  CHECK(!exception_state.HadException());
  CHECK(definition->Descriptor() == descriptor);
  if (RuntimeEnabledFeatures::CustomElementDefaultStyleEnabled() &&
      options->hasStyles())
    definition->SetDefaultStyleSheets(options->styles());
  definitions_.emplace_back(definition);
  NameIdMap::AddResult result = name_id_map_.insert(descriptor.GetName(), id);
  CHECK(result.is_new_entry);

  HeapVector<Member<Element>> candidates;
  CollectCandidates(descriptor, &candidates);
  for (Element* candidate : candidates)
    definition->EnqueueUpgradeReaction(*candidate);

  // 16: when-defined promise processing
  const auto& entry = when_defined_promise_map_.find(name);
  if (entry != when_defined_promise_map_.end()) {
    entry->value->Resolve();
    when_defined_promise_map_.erase(entry);
  }

  return definition;
}

// https://html.spec.whatwg.org/C/#dom-customelementsregistry-get
ScriptValue CustomElementRegistry::get(const AtomicString& name) {
  CustomElementDefinition* definition = DefinitionForName(name);
  if (!definition) {
    // Binding layer converts |ScriptValue()| to script specific value,
    // e.g. |undefined| for v8.
    return ScriptValue();
  }
  return definition->GetConstructorForScript();
}

// https://html.spec.whatwg.org/C/#look-up-a-custom-element-definition
// At this point, what the spec calls 'is' is 'name' from desc
CustomElementDefinition* CustomElementRegistry::DefinitionFor(
    const CustomElementDescriptor& desc) const {
  // desc.name() is 'is' attribute
  // 4. If definition in registry with name equal to local name...
  CustomElementDefinition* definition = DefinitionForName(desc.LocalName());
  // 5. If definition in registry with name equal to name...
  if (!definition)
    definition = DefinitionForName(desc.GetName());
  // 4&5. ...and local name equal to localName, return that definition
  if (definition and definition->Descriptor().LocalName() == desc.LocalName()) {
    return definition;
  }
  // 6. Return null
  return nullptr;
}

bool CustomElementRegistry::NameIsDefined(const AtomicString& name) const {
  return name_id_map_.Contains(name);
}

void CustomElementRegistry::Entangle(V0CustomElementRegistrationContext* v0) {
  v0_->insert(v0);
  v0->SetV1(this);
}

bool CustomElementRegistry::V0NameIsDefined(const AtomicString& name) {
  for (const auto& v0 : *v0_) {
    if (v0->NameIsDefined(name))
      return true;
  }
  return false;
}

CustomElementDefinition* CustomElementRegistry::DefinitionForName(
    const AtomicString& name) const {
  return DefinitionForId(name_id_map_.at(name));
}

CustomElementDefinition* CustomElementRegistry::DefinitionForId(
    CustomElementDefinition::Id id) const {
  return id ? definitions_[id - 1].Get() : nullptr;
}

void CustomElementRegistry::AddCandidate(Element& candidate) {
  AtomicString name = candidate.localName();
  if (!CustomElement::IsValidName(name)) {
    const AtomicString& is = candidate.IsValue();
    if (!is.IsNull())
      name = is;
  }
  if (NameIsDefined(name) || V0NameIsDefined(name))
    return;
  UpgradeCandidateMap::iterator it = upgrade_candidates_->find(name);
  UpgradeCandidateSet* set;
  if (it != upgrade_candidates_->end()) {
    set = it->value;
  } else {
    set = upgrade_candidates_
              ->insert(name, MakeGarbageCollected<UpgradeCandidateSet>())
              .stored_value->value;
  }
  set->insert(&candidate);
}

// https://html.spec.whatwg.org/C/#dom-customelementsregistry-whendefined
ScriptPromise CustomElementRegistry::whenDefined(
    ScriptState* script_state,
    const AtomicString& name,
    ExceptionState& exception_state) {
  if (ThrowIfInvalidName(name, false, exception_state))
    return ScriptPromise();
  CustomElementDefinition* definition = DefinitionForName(name);
  if (definition)
    return ScriptPromise::CastUndefined(script_state);
  ScriptPromiseResolver* resolver = when_defined_promise_map_.at(name);
  if (resolver)
    return resolver->Promise();
  auto* new_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  when_defined_promise_map_.insert(name, new_resolver);
  return new_resolver->Promise();
}

void CustomElementRegistry::CollectCandidates(
    const CustomElementDescriptor& desc,
    HeapVector<Member<Element>>* elements) {
  UpgradeCandidateMap::iterator it = upgrade_candidates_->find(desc.GetName());
  if (it == upgrade_candidates_->end())
    return;
  CustomElementUpgradeSorter sorter;
  for (Element* element : *it.Get()->value) {
    if (!element || !desc.Matches(*element))
      continue;
    sorter.Add(element);
  }

  upgrade_candidates_->erase(it);

  Document* document = owner_->document();
  if (!document)
    return;

  sorter.Sorted(elements, document);
}

// https://html.spec.whatwg.org/C/#dom-customelementregistry-upgrade
void CustomElementRegistry::upgrade(Node* root) {
  DCHECK(root);

  // 1. Let candidates be a list of all of root's shadow-including
  // inclusive descendant elements, in tree order.
  HeapVector<Member<Element>> candidates;
  CollectUpgradeCandidateInNode(*root, candidates);

  // 2. For each candidate of candidates, try to upgrade candidate.
  for (auto& candidate : candidates) {
    CustomElement::TryToUpgrade(*candidate,
                                true /* upgrade_invisible_elements */);
  }
}

}  // namespace blink
