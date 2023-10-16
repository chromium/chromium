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
#include "third_party/blink/renderer/bindings/core/v8/v8_get_inner_html_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_array_css_style_sheet.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsShadowRoot : public DocumentFragment,
                              public TreeScope,
                              public ElementRareDataField {
  Member<void*> member[2];
  unsigned flags[1];
};

ASSERT_SIZE(ShadowRoot, SameSizeAsShadowRoot);

ShadowRoot::ShadowRoot(Document& document, ShadowRootType type)
    : DocumentFragment(nullptr, kCreateShadowRoot),
      TreeScope(
          *this,
          document,
          static_cast<V8ObservableArrayCSSStyleSheet::SetAlgorithmCallback>(
              &ShadowRoot::OnAdoptedStyleSheetSet),
          static_cast<V8ObservableArrayCSSStyleSheet::DeleteAlgorithmCallback>(
              &ShadowRoot::OnAdoptedStyleSheetDelete)),
      child_shadow_root_count_(0),
      type_(static_cast<unsigned>(type)),
      registered_with_parent_shadow_root_(false),
      delegates_focus_(false),
      slot_assignment_mode_(static_cast<unsigned>(SlotAssignmentMode::kNamed)),
      needs_dir_auto_attribute_update_(false),
      has_focusgroup_attribute_on_descendant_(false),
      unused_(0) {}

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
                        ExceptionState&) const {
  NOTREACHED() << "ShadowRoot nodes are not clonable.";
  return nullptr;
}

void ShadowRoot::SetSlotAssignmentMode(SlotAssignmentMode assignment_mode) {
  slot_assignment_mode_ = static_cast<unsigned>(assignment_mode);
}

String ShadowRoot::innerHTML() const {
  return CreateMarkup(this, kChildrenOnly);
}

// This forwards to the TreeScope implementation.
void ShadowRoot::OnAdoptedStyleSheetSet(
    ScriptState* script_state,
    V8ObservableArrayCSSStyleSheet& observable_array,
    uint32_t index,
    Member<CSSStyleSheet>& sheet,
    ExceptionState& exception_state) {
  TreeScope::OnAdoptedStyleSheetSet(script_state, observable_array, index,
                                    sheet, exception_state);
}

// This forwards to the TreeScope implementation.
void ShadowRoot::OnAdoptedStyleSheetDelete(
    ScriptState* script_state,
    V8ObservableArrayCSSStyleSheet& observable_array,
    uint32_t index,
    ExceptionState& exception_state) {
  TreeScope::OnAdoptedStyleSheetDelete(script_state, observable_array, index,
                                       exception_state);
}

String ShadowRoot::getInnerHTML(const GetInnerHTMLOptions* options) const {
  ClosedRootsSet include_closed_roots;
  if (options->hasClosedRoots()) {
    for (auto& shadow_root : options->closedRoots()) {
      include_closed_roots.insert(shadow_root);
    }
  }
  return CreateMarkup(
      this, kChildrenOnly, kDoNotResolveURLs,
      options->includeShadowRoots() ? kIncludeShadowRoots : kNoShadowRoots,
      include_closed_roots);
}

void ShadowRoot::setInnerHTML(const String& html,
                              ExceptionState& exception_state) {
  if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
          html, &host(), kAllowScriptingContent,
          /*include_shadow_roots=*/false, exception_state)) {
    ReplaceChildrenWithFragment(this, fragment, exception_state);
    if (auto* element = DynamicTo<HTMLElement>(host()))
      element->AdjustDirectionalityIfNeededAfterShadowRootChanged();
  }
}

void ShadowRoot::setHTMLUnsafe(const String& html,
                               ExceptionState& exception_state) {
  if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
          html, &host(), kAllowScriptingContent,
          /*include_shadow_roots=*/true, exception_state)) {
    ReplaceChildrenWithFragment(this, fragment, exception_state);
    if (auto* element = DynamicTo<HTMLElement>(host())) {
      element->AdjustDirectionalityIfNeededAfterShadowRootChanged();
    }
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

  if (change.IsChildElementChange()) {
    CheckForSiblingStyleChanges(
        change.type == ChildrenChangeType::kElementRemoved
            ? kSiblingElementRemoved
            : kSiblingElementInserted,
        To<Element>(change.sibling_changed), change.sibling_before_change,
        change.sibling_after_change);
  }
}

void ShadowRoot::SetRegistry(CustomElementRegistry* registry) {
  DCHECK(!registry_);
  DCHECK(!registry ||
         RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled());
  registry_ = registry;
  if (registry) {
    registry->AssociatedWith(GetDocument());
  }
}

void ShadowRoot::Trace(Visitor* visitor) const {
  visitor->Trace(slot_assignment_);
  visitor->Trace(registry_);
  ElementRareDataField::Trace(visitor);
  TreeScope::Trace(visitor);
  DocumentFragment::Trace(visitor);
}

std::ostream& operator<<(std::ostream& ostream, const ShadowRootType& type) {
  switch (type) {
    case ShadowRootType::kUserAgent:
      ostream << "UserAgent";
      break;
    case ShadowRootType::kOpen:
      ostream << "Open";
      break;
    case ShadowRootType::kClosed:
      ostream << "Closed";
      break;
  }
  return ostream;
}

}  // namespace blink
