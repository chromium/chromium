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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SHADOW_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SHADOW_ROOT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class Document;
class ExceptionState;
class ShadowRootV0;
class SlotAssignment;
class StringOrTrustedHTML;
class WhitespaceAttacher;

enum class ShadowRootType { V0, kOpen, kClosed, kUserAgent };

enum class ShadowRootSlotting { kManual, kAuto };

class CORE_EXPORT ShadowRoot final : public DocumentFragment, public TreeScope {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ShadowRoot);

 public:
  ShadowRoot(Document&, ShadowRootType);

  // Disambiguate between Node and TreeScope hierarchies; TreeScope's
  // implementation is simpler.
  using TreeScope::GetDocument;
  using TreeScope::getElementById;

  // Make protected methods from base class public here.
  using TreeScope::SetDocument;
  using TreeScope::SetParentTreeScope;

  Element& host() const {
    DCHECK(ParentOrShadowHostNode());
    return *To<Element>(ParentOrShadowHostNode());
  }
  ShadowRootType GetType() const { return static_cast<ShadowRootType>(type_); }
  String mode() const {
    switch (GetType()) {
      case ShadowRootType::kUserAgent:
        // UA ShadowRoot should not be exposed to the Web.
        NOTREACHED();
        return "";
      case ShadowRootType::V0:
        // v0 ShadowRoot shouldn't support |mode|, however, we must return
        // something. Return "open" here for a historical reason.
        return "open";
      case ShadowRootType::kOpen:
        return "open";
      case ShadowRootType::kClosed:
        return "closed";
      default:
        NOTREACHED();
        return "";
    }
  }

  bool IsV0() const { return GetType() == ShadowRootType::V0; }
  bool IsOpenOrV0() const {
    return GetType() == ShadowRootType::V0 ||
           GetType() == ShadowRootType::kOpen;
  }
  bool IsV1() const {
    return GetType() == ShadowRootType::kOpen ||
           GetType() == ShadowRootType::kClosed ||
           GetType() == ShadowRootType::kUserAgent;
  }
  bool IsUserAgent() const { return GetType() == ShadowRootType::kUserAgent; }

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  void SetNeedsAssignmentRecalc();
  bool NeedsSlotAssignmentRecalc() const;

  ShadowRootV0& V0() const;

  // For Internals, don't use this.
  unsigned ChildShadowRootCount() const { return child_shadow_root_count_; }

  void RecalcStyle(const StyleRecalcChange);
  void RebuildLayoutTree(WhitespaceAttacher&);

  void RegisterScopedHTMLStyleChild();
  void UnregisterScopedHTMLStyleChild();

  SlotAssignment& GetSlotAssignment() {
    DCHECK(slot_assignment_);
    return *slot_assignment_;
  }

  bool HasSlotAssignment() { return slot_assignment_; }

  HTMLSlotElement* AssignedSlotFor(const Node&);
  void DidAddSlot(HTMLSlotElement&);
  void DidChangeHostChildSlotName(const AtomicString& old_value,
                                  const AtomicString& new_value);

  void SetNeedsDistributionRecalcWillBeSetNeedsAssignmentRecalc();
  void SetNeedsDistributionRecalc();
  bool NeedsDistributionRecalc() const { return needs_distribution_recalc_; }

  void DistributeIfNeeded();

  Element* ActiveElement() const;

  String InnerHTMLAsString() const;
  void SetInnerHTMLFromString(const String&,
                              ExceptionState& = ASSERT_NO_EXCEPTION);

  // TrustedTypes variants of the above.
  // TODO(mkwst): Write a spec for these bits. https://crbug.com/739170
  void innerHTML(StringOrTrustedHTML&) const;
  void setInnerHTML(const StringOrTrustedHTML&, ExceptionState&);

  Node* Clone(Document&, CloneChildrenFlag) const override;

  void SetDelegatesFocus(bool flag) { delegates_focus_ = flag; }
  bool delegatesFocus() const { return delegates_focus_; }

  void SetSlotting(ShadowRootSlotting slotting);
  bool IsManualSlotting() {
    return slotting_ == static_cast<unsigned>(ShadowRootSlotting::kManual);
  }

  bool ContainsShadowRoots() const { return child_shadow_root_count_; }

  StyleSheetList& StyleSheets();
  void SetStyleSheets(StyleSheetList* style_sheet_list) {
    style_sheet_list_ = style_sheet_list;
  }

  void Trace(Visitor*) override;

 private:
  ~ShadowRoot() override;

  void ChildrenChanged(const ChildrenChange&) override;

  SlotAssignment& EnsureSlotAssignment();

  void AddChildShadowRoot() { ++child_shadow_root_count_; }
  void RemoveChildShadowRoot() {
    DCHECK_GT(child_shadow_root_count_, 0u);
    --child_shadow_root_count_;
  }
  void Distribute();

  Member<StyleSheetList> style_sheet_list_;
  Member<SlotAssignment> slot_assignment_;
  Member<ShadowRootV0> shadow_root_v0_;
  unsigned child_shadow_root_count_ : 16;
  unsigned type_ : 2;
  unsigned registered_with_parent_shadow_root_ : 1;
  unsigned delegates_focus_ : 1;
  unsigned slotting_ : 1;
  unsigned needs_distribution_recalc_ : 1;
  unsigned unused_ : 10;

  DISALLOW_COPY_AND_ASSIGN(ShadowRoot);
};

inline Element* ShadowRoot::ActiveElement() const {
  return AdjustedFocusedElement();
}

inline bool Node::IsInUserAgentShadowRoot() const {
  return ContainingShadowRoot() && ContainingShadowRoot()->IsUserAgent();
}

inline void ShadowRoot::DistributeIfNeeded() {
  if (needs_distribution_recalc_)
    Distribute();
  needs_distribution_recalc_ = false;
}

inline ShadowRoot* Node::GetShadowRoot() const {
  auto* this_element = DynamicTo<Element>(this);
  if (!this_element)
    return nullptr;
  return this_element->GetShadowRoot();
}

inline ShadowRoot* Element::ShadowRootIfV1() const {
  ShadowRoot* root = GetShadowRoot();
  if (root && root->IsV1())
    return root;
  return nullptr;
}

inline ShadowRootV0& ShadowRoot::V0() const {
  DCHECK(shadow_root_v0_);
  DCHECK(IsV0());
  return *shadow_root_v0_;
}

template <>
struct DowncastTraits<ShadowRoot> {
  static bool AllowFrom(const Node& node) { return node.IsShadowRoot(); }

  static bool AllowFrom(const TreeScope& tree_scope) {
    return tree_scope.RootNode().IsShadowRoot();
  }
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const ShadowRootType&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SHADOW_ROOT_H_
