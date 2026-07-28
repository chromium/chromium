// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_MUTATION_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_MUTATION_OBSERVER_H_

#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"

namespace blink {

class MenuMutationObserver : public MutationObserver::Delegate {
 public:
  explicit MenuMutationObserver(HTMLMenuOwnerElement& menu_owner);

  ExecutionContext* GetExecutionContext() const override;
  void Deliver(const MutationRecordVector& records, MutationObserver&) override;
  void Trace(Visitor* visitor) const override;

  void Disconnect();

 private:
  bool IsRecordOwnedByThisMenu(
      MutationRecord* record,
      const HeapHashSet<Member<const Node>>& detached_roots_we_own) const;
  void CheckAddedNodes(MutationRecord* record,
                       HeapHashSet<Member<Node>>& visited_nodes);
  void CheckRemovedNodes(MutationRecord* record);
  void TraverseNodeDescendants(const Node* node,
                               HeapHashSet<Member<Node>>& visited_nodes);
  void AddDescendantDisallowedErrorToNode(
      Node& node,
      HeapHashSet<Member<Node>>& visited_nodes);
  ElementAccessibilityIssueReason CheckForIssue(
      const Node& descendant,
      const Node* disconnected_root = nullptr,
      const Node* disconnected_parent = nullptr);

  Member<HTMLMenuOwnerElement> menu_owner_;
  Member<MutationObserver> observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_MUTATION_OBSERVER_H_
