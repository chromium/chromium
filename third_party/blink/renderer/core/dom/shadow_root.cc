/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include "third_party/blink/renderer/core/dom/shadow_root.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/module_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_array_css_style_sheet.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_unsafe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_mode.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_slot_assignment_mode.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_stringlegacynulltoemptystring_trustedhtml.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/script/value_wrapper_synthetic_module_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

class ReferenceTargetIdObserver : public IdTargetObserver {
 public:
  ReferenceTargetIdObserver(const AtomicString& id, ShadowRoot* root)
      : IdTargetObserver(root->EnsureIdTargetObserverRegistry(), id),
        root_(root) {}

  using IdTargetObserver::Id;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(root_);
    IdTargetObserver::Trace(visitor);
  }

  void IdTargetChanged() override { root_->ReferenceTargetChanged(); }

 private:
  Member<ShadowRoot> root_;
};

struct SameSizeAsShadowRoot : public DocumentFragment,
                              public TreeScope,
                              public ElementRareDataField {
  Member<void*> member[2];
  unsigned flags[1];
};

ASSERT_SIZE(ShadowRoot, SameSizeAsShadowRoot);

ShadowRoot::ShadowRoot(Document& document,
                       ShadowRootMode mode,
                       SlotAssignmentMode assignment_mode)
    : DocumentFragment(nullptr, kCreateShadowRoot),
      TreeScope(*this, document),
      child_shadow_root_count_(0),
      mode_(static_cast<unsigned>(mode)),
      registered_with_parent_shadow_root_(false),
      delegates_focus_(false),
      slot_assignment_mode_(static_cast<unsigned>(assignment_mode)),
      has_focusgroup_attribute_on_descendant_(false) {}

ShadowRoot::~ShadowRoot() = default;

SlotAssignment& ShadowRoot::EnsureSlotAssignment() {
  if (!slot_assignment_)
    slot_assignment_ = MakeGarbageCollected<SlotAssignment>(*this);
  return *slot_assignment_;
}

HTMLSlotElement* ShadowRoot::AssignedSlotFor(const Node& node) {
  if (!slot_assignment_)
    return nullptr;
  return slot_assignment_->FindSlot(node);
}

void ShadowRoot::DidAddSlot(HTMLSlotElement& slot) {
  EnsureSlotAssignment().DidAddSlot(slot);
}

void ShadowRoot::DidChangeHostChildSlotName(const AtomicString& old_value,
                                            const AtomicString& new_value) {
  if (!slot_assignment_)
    return;
  slot_assignment_->DidChangeHostChildSlotName(old_value, new_value);
}

Node* ShadowRoot::Clone(Document&,
                        NodeCloningData&,
                        ContainerNode*,
                        CustomElementRegistry*,
                        ExceptionState&) const {
  NOTREACHED() << "ShadowRoot nodes are not clonable.";
}

String ShadowRoot::GetInnerHTMLString() const {
  return CreateMarkup(this, kChildrenOnly);
}

V8UnionStringLegacyNullToEmptyStringOrTrustedHTML* ShadowRoot::innerHTML()
    const {
  return MakeGarbageCollected<
      V8UnionStringLegacyNullToEmptyStringOrTrustedHTML>(GetInnerHTMLString());
}

void ShadowRoot::SetInnerHTMLWithoutTrustedTypes(
    const String& html,
    ExceptionState& exception_state) {
  if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
          html, &host(), kAllowScriptingContent,
          Element::ParseDeclarativeShadowRoots::kDontParse,
          Element::ForceHtml::kDontForce, ForceInertTemplate::kDontForce,
          customElementRegistry(), exception_state)) {
    ReplaceChildrenWithFragment(this, fragment, exception_state);
  }
}

void ShadowRoot::setInnerHTML(
    const V8UnionStringLegacyNullToEmptyStringOrTrustedHTML* html,
    ExceptionState& exception_state) {
  String compliant_html = TrustedTypesCheckForHTML(
      html, GetExecutionContext(), trusted_types_names::kShadowRoot,
      trusted_types_names::kInnerHTML, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  SetInnerHTMLWithoutTrustedTypes(compliant_html, exception_state);
}

void ShadowRoot::setHTMLUnsafe(const V8UnionStringOrTrustedHTML* html,
                               ExceptionState& exception_state) {
  UseCounter::Count(GetDocument(), WebFeature::kHTMLUnsafeMethods);
  String compliant_html = TrustedTypesCheckForHTML(
      html, GetExecutionContext(), trusted_types_names::kShadowRoot,
      trusted_types_names::kSetHTMLUnsafe, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
          compliant_html, &host(), kAllowScriptingContent,
          Element::ParseDeclarativeShadowRoots::kParse,
          Element::ForceHtml::kDontForce, ForceInertTemplate::kForce,
          customElementRegistry(), exception_state)) {
    if (RuntimeEnabledFeatures::SanitizerAPIEnabled()) {
      SanitizerAPI::SanitizeUnsafeInternal(this, fragment, nullptr,
                                           exception_state);
    }
    ReplaceChildrenWithFragment(this, fragment, exception_state);
  }
}

void ShadowRoot::setHTMLUnsafe(const V8UnionStringOrTrustedHTML* html,
                               SetHTMLUnsafeOptions* options,
                               ExceptionState& exception_state) {
  String compliant_html = TrustedTypesCheckForHTML(
      html, GetExecutionContext(), trusted_types_names::kShadowRoot,
      trusted_types_names::kSetHTMLUnsafe, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
          compliant_html, &host(),
          RuntimeEnabledFeatures::SetHTMLCanRunScriptsEnabled() &&
                  options->runScripts()
              ? kAllowScriptingContentAndDoNotMarkAlreadyStarted
              : kAllowScriptingContent,
          Element::ParseDeclarativeShadowRoots::kParse,
          Element::ForceHtml::kDontForce, ForceInertTemplate::kForce,
          customElementRegistry(), exception_state)) {
    if (RuntimeEnabledFeatures::SanitizerAPIEnabled()) {
      SanitizerAPI::SanitizeUnsafeInternal(this, fragment, options,
                                           exception_state);
    }
    ReplaceChildrenWithFragment(this, fragment, exception_state);
  }
}

void ShadowRoot::setHTML(const String& html,
                         SetHTMLOptions* options,
                         ExceptionState& exception_state) {
  if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
          html, &host(), kAllowScriptingContent,
          Element::ParseDeclarativeShadowRoots::kParse,
          Element::ForceHtml::kDontForce, ForceInertTemplate::kForce,
          customElementRegistry(), exception_state)) {
    if (RuntimeEnabledFeatures::SanitizerAPIEnabled()) {
      SanitizerAPI::SanitizeSafeInternal(this, fragment, options,
                                         exception_state);
    }
    ReplaceChildrenWithFragment(this, fragment, exception_state);
  }
}

void ShadowRoot::RebuildLayoutTree(WhitespaceAttacher& whitespace_attacher) {
  DCHECK(!NeedsReattachLayoutTree());
  DCHECK(!ChildNeedsReattachLayoutTree());
  RebuildChildrenLayoutTrees(whitespace_attacher);
}

void ShadowRoot::DetachLayoutTree(bool performing_reattach) {
  ContainerNode::DetachLayoutTree(performing_reattach);

  // Shadow host may contain unassigned light dom children that need detaching.
  // Assigned nodes are detached by the slot element.
  for (Node& child : NodeTraversal::ChildrenOf(host())) {
    if (!child.IsSlotable() || child.AssignedSlotWithoutRecalc())
      continue;

    if (child.GetDocument() == GetDocument())
      child.DetachLayoutTree(performing_reattach);
  }
}

Node::InsertionNotificationRequest ShadowRoot::InsertedInto(
    ContainerNode& insertion_point) {
  DocumentFragment::InsertedInto(insertion_point);

  if (!insertion_point.isConnected())
    return kInsertionDone;

  GetDocument().GetStyleEngine().ShadowRootInsertedToDocument(*this);

  GetDocument().GetSlotAssignmentEngine().Connected(*this);

  // FIXME: When parsing <video controls>, InsertedInto() is called many times
  // without invoking RemovedFrom().  For now, we check
  // registered_with_parent_shadow_root. We would like to
  // DCHECK(!registered_with_parent_shadow_root) here.
  // https://bugs.webkit.org/show_bug.cig?id=101316
  if (registered_with_parent_shadow_root_)
    return kInsertionDone;

  if (ShadowRoot* root = host().ContainingShadowRoot()) {
    root->AddChildShadowRoot();
    registered_with_parent_shadow_root_ = true;
  }

  return kInsertionDone;
}

void ShadowRoot::RemovedFrom(ContainerNode& insertion_point) {
  if (insertion_point.isConnected()) {
    if (NeedsSlotAssignmentRecalc())
      GetDocument().GetSlotAssignmentEngine().Disconnected(*this);
    GetDocument().GetStyleEngine().ShadowRootRemovedFromDocument(this);
    if (registered_with_parent_shadow_root_) {
      ShadowRoot* root = host().ContainingShadowRoot();
      if (!root)
        root = insertion_point.ContainingShadowRoot();
      if (root)
        root->RemoveChildShadowRoot();
      registered_with_parent_shadow_root_ = false;
    }
  }

  DocumentFragment::RemovedFrom(insertion_point);
}

V8ShadowRootMode ShadowRoot::mode() const {
  switch (GetMode()) {
    case ShadowRootMode::kOpen:
      return V8ShadowRootMode(V8ShadowRootMode::Enum::kOpen);
    case ShadowRootMode::kClosed:
      return V8ShadowRootMode(V8ShadowRootMode::Enum::kClosed);
    case ShadowRootMode::kUserAgent:
      // UA ShadowRoot should not be exposed to the Web.
      break;
  }
  NOTREACHED();
}

V8SlotAssignmentMode ShadowRoot::slotAssignment() const {
  return V8SlotAssignmentMode(IsManualSlotting()
                                  ? V8SlotAssignmentMode::Enum::kManual
                                  : V8SlotAssignmentMode::Enum::kNamed);
}

HeapVector<Member<CSSStyleSheet>>
ShadowRoot::GetFetchedStyleSheetsFromModuleMap(
    const AtomicString& shadowrootadoptedstylesheets_attribute_value) {
  CHECK(RuntimeEnabledFeatures::DeclarativeCSSModulesEnabled());

  // Early exit if `domWindow` isn't available. This won't work in contexts such
  // as `Document.parseHTMLUnsafe`. This is probably fine, as adopted
  // stylesheets are cleared when moving between documents (so it wouldn't be
  // able to render the adopted styles anyways). Also,
  // `Document.parseHTMLUnsafe` cannot execute scripts, so this isn't a
  // limitation compared to the imperative version.
  // TODO(448174611): confirm this behavior is correct with the WHATWG.
  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window) {
    return {};
  }

  Modulator* modulator =
      Modulator::From(ToScriptStateForMainWorld(window->GetFrame()));
  v8::Isolate* isolate = modulator->GetScriptState()->GetIsolate();
  CHECK(isolate);

  // Several operations below require a HandleScope.
  v8::HandleScope handle_scope(isolate);

  HeapVector<Member<CSSStyleSheet>> sheets;
  SpaceSplitString specifiers(shadowrootadoptedstylesheets_attribute_value);
  sheets.ReserveInitialCapacity(specifiers.size());

  for (const auto& specifier : specifiers) {
    // Resolve the specifier to ensure import maps are accounted for.
    const KURL resolved_url = modulator->ResolveModuleSpecifier(
        specifier, window->BaseURL(), /*failure_reason=*/nullptr);
    if (resolved_url.IsValid()) {
      // Synchronously fetch dataURI's. These will be processed immediately and
      // available synchronously per https://fetch.spec.whatwg.org/#data-urls.
      // We don't need to do this for Blob URL's generated from a <style
      // type="module"> because they have already been added to the module map.
      // TODO(crbug.com/448174611) - should RequestContextType and
      // RequestDestination be script or style?
      if (resolved_url.ProtocolIsData()) {
        ScriptFetchOptions options;
        ModuleScriptFetchRequest module_request(
            resolved_url, ModuleType::kCSS,
            mojom::blink::RequestContextType::SCRIPT,
            network::mojom::RequestDestination::kScript, options,
            Referrer::ClientReferrerString(), TextPosition::MinimumPosition(),
            ModuleImportPhase::kEvaluation);
        modulator->FetchSingle(module_request, window->Fetcher(),
                               ModuleGraphLevel::kTopLevelModuleFetch,
                               ModuleScriptCustomFetchType::kNone, nullptr);
      }

      const ModuleScript* module_script =
          modulator->GetFetchedModuleScript(resolved_url, ModuleType::kCSS);
      if (module_script) {
        CSSStyleSheet* sheet = V8CSSStyleSheet::ToWrappable(
            isolate,
            static_cast<const ValueWrapperSyntheticModuleScript*>(module_script)
                ->GetExport(isolate));
        CHECK_EQ(sheet->ConstructorDocument(), GetDocument());
        sheets.push_back(*sheet);
      }
    }
  }
  return sheets;
}

void ShadowRoot::ProcessAdoptedStylesheetAttribute(
    AtomicString value) {
  CHECK(RuntimeEnabledFeatures::DeclarativeCSSModulesEnabled());
  if (!value.empty()) {
    AppendAdoptedStyleSheets(GetFetchedStyleSheetsFromModuleMap(value));
  }
}

void ShadowRoot::SetNeedsAssignmentRecalc() {
  if (!slot_assignment_)
    return;
  return slot_assignment_->SetNeedsAssignmentRecalc();
}

bool ShadowRoot::NeedsSlotAssignmentRecalc() const {
  return slot_assignment_ && slot_assignment_->NeedsAssignmentRecalc();
}

void ShadowRoot::ChildrenChanged(const ChildrenChange& change) {
  ContainerNode::ChildrenChanged(change);

  if (change.type ==
      ChildrenChangeType::kFinishedBuildingDocumentFragmentTree) {
    // No need to call CheckForSiblingStyleChanges() as at this point the
    // node is not in the active document (CheckForSiblingStyleChanges() does
    // nothing when not in the active document).
    DCHECK(!InActiveDocument());
  } else if (change.IsChildElementChange()) {
    Element* changed_element = To<Element>(change.sibling_changed);
    bool removed = change.type == ChildrenChangeType::kElementRemoved;
    CheckForSiblingStyleChanges(
        removed ? kSiblingElementRemoved : kSiblingElementInserted,
        changed_element, change.sibling_before_change,
        change.sibling_after_change);
    GetDocument()
        .GetStyleEngine()
        .ScheduleInvalidationsForHasPseudoAffectedByInsertionOrRemoval(
            this, change.sibling_before_change, *changed_element, removed);
  }

  // In the case of input types like button where the child element is not
  // in a container, we need to explicit adjust directionality.
  if (TextControlElement* text_element =
          HTMLElement::ElementIfAutoDirectionalityFormAssociatedOrNull(
              &host())) {
    text_element->AdjustDirectionalityIfNeededAfterChildrenChanged(change);
  }
}

void ShadowRoot::setReferenceTarget(const AtomicString& reference_target) {
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          GetDocument().GetExecutionContext())) {
    return;
  }

  UseCounter::CountWebDXFeature(GetDocument(),
                                WebDXFeature::kDRAFT_ReferenceTarget);

  if (referenceTarget() == reference_target) {
    return;
  }

  const Element* previous_reference_target_element = referenceTargetElement();

  if (reference_target_id_observer_) {
    reference_target_id_observer_->Unregister();
  }

  reference_target_id_observer_ =
      reference_target ? MakeGarbageCollected<ReferenceTargetIdObserver>(
                             reference_target, this)
                       : nullptr;

  if (previous_reference_target_element != referenceTargetElement()) {
    ReferenceTargetChanged();
  }
}

const AtomicString& ShadowRoot::referenceTarget() const {
  return reference_target_id_observer_ ? reference_target_id_observer_->Id()
                                       : g_null_atom;
}

Element* ShadowRoot::referenceTargetElement() const {
  return getElementById(referenceTarget());
}

void ShadowRoot::ReferenceTargetChanged() {
  // When this ShadowRoot's reference target changes, notify anything observing
  // the host element's ID, since they may have been referring to the reference
  // target instead.
  if (const auto& id = host().GetIdAttribute()) {
    if (auto* registry = host().GetTreeScope().GetIdTargetObserverRegistry()) {
      registry->NotifyObservers(id);
    }
  }

  if (host().isConnected()) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      cache->HandleReferenceTargetChanged(host());
    }
  }
}

void ShadowRoot::Trace(Visitor* visitor) const {
  visitor->Trace(slot_assignment_);
  visitor->Trace(reference_target_id_observer_);
  ElementRareDataField::Trace(visitor);
  TreeScope::Trace(visitor);
  DocumentFragment::Trace(visitor);
}

std::ostream& operator<<(std::ostream& ostream, const ShadowRootMode& mode) {
  switch (mode) {
    case ShadowRootMode::kUserAgent:
      ostream << "UserAgent";
      break;
    case ShadowRootMode::kOpen:
      ostream << "Open";
      break;
    case ShadowRootMode::kClosed:
      ostream << "Closed";
      break;
  }
  return ostream;
}

}  // namespace blink
