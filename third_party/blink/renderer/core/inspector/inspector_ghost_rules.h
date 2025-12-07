// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_GHOST_RULES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_GHOST_RULES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/quiet_mutation_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSNestedDeclarationsRule;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class ExecutionContext;
class TreeScope;
class InspectorGhostRuleTest;

// Ghost Rules are style rules that exist only during getMatchedStylesForNode
// in order to surface the CSSNestedDeclaration rules that *could* be there,
// if the author takes some action, or another user-agent was used.
//
// For example:
//
//  .a {
//    .b {}
//    /* border: 1px solid black */
//    .c {}
//    -webkit-thing: auto;
//  }
//
// Here, no CSSNestedDeclaration rules are actually emitted, because there are
// no valid declarations. Yet, it's useful for the author to see the
// CSSNestedDeclaration rules that would be there, had "border:1px solid black"
// been uncommented, or "-webkit-thing" supported.
//
// Therefore, a CSSNestedDeclarations rule is temporarily inserted into the page
// stylesheet at any location where such a rule *could* appear.
//
// Note also that when parsing stylesheets for the inspector,
// CSSNestedDeclarations are always emitted, see
// CSSParserImpl::EmitNestedDeclarationsRuleIfNeeded. However, this does not
// affect the page stylesheet (the basis for what getMatchedStylesForNode
// returns), hence this class.
class CORE_EXPORT InspectorGhostRules {
  STACK_ALLOCATED();

 public:
  // Attempts to call Populate() on every sheet.
  //
  // A return value of 'true' means every sheet was populated, and a return
  // value of 'false' means *some* CSSStyleSheets were skipped due to invalid
  // StyleSheetContents sharing (crbug.com/417619104). Some sheets being skipped
  // is non-fatal: those sheets will just not contain any ghost rules, but are
  // as normal otherwise.
  [[nodiscard]] bool PopulateSheets(HeapVector<Member<CSSStyleSheet>>);

  // Like PopulateSheets, does DCHECK+DumpWithoutCrashing on failure.
  // TODO(crbug.com/417619104): Remove when investigation is done.
  void PopulateSheetsWithAssertion(HeapVector<Member<CSSStyleSheet>>);

  // Temporarily make rules inserted by `Populate` available for rule matching.
  // Like `Populate`, this is a "quiet" process, causing no invalidation.
  // Any changes made to the StyleEngine are quietly undone by the destructor.
  void Activate(Document&);

  ~InspectorGhostRules();

  bool Contains(CSSStyleRule* rule) const {
    return inner_rules_.Contains(rule);
  }

 private:
  friend class InspectorGhostRuleTest;

  // Insert CSSNestedDeclaration rules in any place where they can occur.
  //
  // Note that this *quietly* inserts rules (see CSSStyleRule/CSSGroupingRule::
  // QuietlyInsertRule), which means that no style invalidation will take place
  // as a result of calling this function. The ghost rules are instead made
  // available for rule matching by `Activate`.
  void Populate(CSSStyleSheet&);

  void PopulateSheet(const ExecutionContext&, CSSStyleSheet&);
  void DepopulateSheet(CSSStyleSheet&);

  void ActivateTreeScope(TreeScope&);

  QuietMutationScope quiet_mutation_scope_;
  HeapHashSet<Member<CSSStyleSheet>> affected_stylesheets_;
  HeapHashSet<Member<CSSNestedDeclarationsRule>> inserted_rules_;
  // The inner CSSStyleRule for each item in `inserted_rules_`.
  HeapHashSet<Member<CSSStyleRule>> inner_rules_;

  HeapHashMap<Member<TreeScope>, ActiveStyleSheetVector> affected_tree_scopes;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_GHOST_RULES_H_
