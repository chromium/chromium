---
name: ios/document-non-obvious-conditions-and-state-transitions
description: When writing non-obvious branch conditions, sentinel guards, early-return bypasses, or complex traversal heuristics, document underlying invariants, delegation or bailout rationale, and concrete walkthrough scenarios
rubric: readability
severity: nit
citations:
  - path: docs/documentation_best_practices.md
    heading: Documentation is the story of your code
    quote: The primary purpose of inline comments is to provide information that the code itself cannot contain, such as why the code is there.
  - path: docs/documentation_guidelines.md
    heading: Implementation documentation
    quote: A good rule of thumb is that if a code reviewer had a clarifying question about something during review and the code couldn't be simplified to answer the question then a comment is probably appropriate.
  - path: agents/prompts/common.minimal.md
    heading: Coding
    quote: 'Add code comments sparingly: Focus on *why* something is done, not *what* is done.'
  - path: docs/documentation_best_practices.md
    heading: Inline comments
    quote: The primary purpose of inline comments is to provide information that the code itself cannot contain, such as why the code is there.
  - path: styleguide/c++/checks.md
    heading: CHECK(), DCHECK() and NOTREACHED()
    quote: CHECK(), DCHECK() and NOTREACHED() are all used to ensure that invariants hold. They document (and verify) programmer expectations that either some statement always holds true at the point of (D)CHECKing or that a piece of code is unreachable (for NOTREACHED).
---

**Rule.** Add an explanatory inline comment directly above non-obvious boolean
conditions, sentinel checks, domain-specific thresholds, derived state flags,
early-return guards, failable initializers, operation bypasses, multi-branch
loops, non-obvious `CHECK`ed preconditions, unconditional UI side effects,
hardcoded parameters where dynamic evaluation is expected, and queries of global
or session state in place of in-scope parameters, whenever the underlying logic,
lifecycle state, invariant, or failure mode is not evident from the syntax
alone. Document what conditions or sentinel values signify, what missing
prerequisite prevents initialization, why execution is bypassed, why a
restriction is required (not merely a paraphrase of the asserted condition), why
an action is unconditional, what scenario produces an unexpected state, or why a
parameter is fixed. In particular:

- When implementing complex heuristics, multi-branch traversals, or skipping
  logic in loops, document why items are skipped and include a concrete
  walkthrough scenario illustrating the state transition (e.g., auto-created
  navigation items).
- When an early return or guard bypasses default processing because an upstream
  delegate or subsystem already consumed the event (such as
  `HandleSessionOpenURLContexts`), document that it was handled upstream to
  prevent duplicate processing; when processing is deferred to a downstream
  delegate or secondary handler (such as a drop handler), document where and how
  it is handled.
- When a conditional check (such as a null check on a service like
  `TemplateURLService`) represents a distinct domain state rather than routine
  defensive error handling, clarify the state and why fallback logic or strings
  are selected.
- When an expected action or UI dismissal is guarded by a condition (such as
  delegate acceptance like `passwordDelegate->Accept()`), or when observer
  callbacks exclude specific lifecycle events (such as
  `IsVisibleURLNewTabPage`), explain the active flow constraint or UI-hierarchy
  invariant requiring the bypass.
- When enforcing a non-obvious precondition, lifecycle constraint, or
  profile/mode restriction with `CHECK` (such as
  `CHECK(!profile->IsOffTheRecord())`), document why the restriction exists;
  when behavior intentionally diverges from another platform or reads session
  state instead of a parameter, say why.

**Why.** Non-obvious conditional checks, skipping loops, sentinel values,
early-return guards, asserted invariants, and unconditional or hardcoded
operations obscure developer intent and increase cognitive load. Without
explanatory inline comments and concrete walkthrough scenarios, reviewers and
future maintainers cannot distinguish intentional domain handling, lifecycle
constraints, architectural rationale (such as why a coordinator or service
cannot run off-the-record, or why desktop and iOS behaviors diverge), or
view-hierarchy edge cases from accidental omissions, dead code, or routine
defensive checks. Documenting why execution is bypassed (handled upstream or
deferred downstream), why a restriction or unconditional action exists, and how
complex traversal heuristics behave prevents invalid assumptions, avoids
duplicate processing, and protects non-obvious control-flow invariants and
safeguards during future refactorings.

## Examples

Bad:

```cpp
// 1. Unexplained skipping traversal heuristic:
while (item && item->WasCreatedAutomatically()) {
  index += delta;
  item = GetNavigationItemImplAtIndex(index);
}

// 2. Unexplained early return when upstream handler already consumed event:
if (GetApplicationContext()
        ->GetSystemIdentityManager()
        ->HandleSessionOpenURLContexts(sceneState.scene, URLContextSet)) {
  return;
}

// 3. Unexplained domain state fallback or guarded action:
if (!_templateURLService || !_templateURLService->GetDefaultSearchProvider()) {
  return l10n_util::GetNSString(IDS_SUBTITLE_UNKNOWN_DSE);
}
if (self.passwordDelegate->Accept()) {
  [self dismissOverlay];
}

// 4. Asserted restriction, unconditional UI action, and global query without
// rationale:
CHECK(!profile->IsOffTheRecord());
[_collectionView selectItemAtIndexPath:defaultIndexPath
                              animated:NO
                        scrollPosition:UICollectionViewScrollPositionNone];
conditions = service->GetConditions({.allow_unknown_location = false});
RecordDebugTriggering(cond, /*is_first_run=*/FirstRun::IsChromeFirstRun());
```

Good:

```cpp
// 1. Walkthrough scenario explaining skipping traversal logic:
// Say we have: a.com -> b.com (auto) -> c.com. Backward navigation skips
// auto-created items to reach the preceding user-initiated navigation.
while (item && item->WasCreatedAutomatically()) {
  index += delta;
  item = GetNavigationItemImplAtIndex(index);
}

// 2. Document upstream handling to explain early return:
// If SystemIdentityManager handled the URL context, return early to avoid
// opening the URL twice.
if (GetApplicationContext()
        ->GetSystemIdentityManager()
        ->HandleSessionOpenURLContexts(sceneState.scene, URLContextSet)) {
  return;
}

// 3. Clarify domain state fallback and guarded UI dismissal:
// Sets fallback subtitle when the default search engine is null or unknown.
if (!_templateURLService || !_templateURLService->GetDefaultSearchProvider()) {
  return l10n_util::GetNSString(IDS_SUBTITLE_UNKNOWN_DSE);
}
// Dismiss overlay only if there is no ongoing password error fix flow.
if (self.passwordDelegate->Accept()) {
  [self dismissOverlay];
}

// 4. Document the restriction, the invariant behind the unconditional action,
// and the platform divergence / session-state query:
// Clear browsing data details are only available in regular browsing mode.
CHECK(!profile->IsOffTheRecord());
// Since a custom color is applied, the custom cell must be initially selected.
[_collectionView selectItemAtIndexPath:defaultIndexPath
                              animated:NO
                        scrollPosition:UICollectionViewScrollPositionNone];
// Unlike Desktop, on iOS location must be known regardless of FRE.
conditions = service->GetConditions({.allow_unknown_location = false});
// Query session state directly because eligibility depends on session lifecycle.
RecordDebugTriggering(cond, /*is_first_run=*/FirstRun::IsChromeFirstRun());
```
