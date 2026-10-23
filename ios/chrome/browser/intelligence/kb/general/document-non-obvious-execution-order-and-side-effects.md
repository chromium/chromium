---
name: ios/document-non-obvious-execution-order-and-side-effects
description: When operations, state updates, or teardowns have load-bearing execution order, document the sequencing rationale, explicitly stating lifecycle invariants in cleanup, and avoid gratuitous reordering
rubric: readability
severity: should_fix
citations:
  - path: docs/ui/learn/bestpractices/ownership.md
    heading: Keep lifetimes fully nested
    quote: For objects you own, **destroy in the reverse order you created,** so lifetimes are nested rather than partially-overlapping. This can also reduce the likelihood of use-after-free
  - path: docs/documentation_best_practices.md
    heading: Documentation is the story of your code
    quote: The primary purpose of inline comments is to provide information that the code itself cannot contain, such as why the code is there.
  - path: docs/documentation_guidelines.md
    heading: Implementation documentation
    quote: A good rule of thumb is that if a code reviewer had a clarifying question about something during review and the code couldn't be simplified to answer the question then a comment is probably appropriate.
---

**Rule.** When the execution, initialization, traversal, state mutation, or
teardown order of consecutive operations is load-bearing—such as preventing
subtle framework side effects (forced layout passes, synchronous screen
updates), ensuring prerequisite subviews or child controllers are fully
configured before presentation, committing data or clearing text before exiting
UI states to prevent stale recalculations, avoiding re-entrant callback cycles
in cleanup routines, resolving timing constraints across startup modes, or
avoiding index shifting during reverse removals—add an inline comment explaining
why that specific sequence is required and cite any relevant public issue (e.g.,
crbug.com/123456). Additionally, when documenting ordering constraints in
teardown, shutdown, or cleanup routines, explicitly state the underlying
lifecycle invariant (such as mirroring the reverse order of creation, startup,
or registration) rather than only describing an immediate symptom, workaround,
or downstream consequence (e.g., removing views while an object is still valid).
Conversely, do not gratuitously reorder existing function or method calls if
their relative execution order does not affect functionality; preserve original
call order to avoid diff churn unless a functional dependency requires the
change.

**Why.** Operations, state updates, startup sequences, teardown routines, or
collection removals whose ordering is dictated by framework side effects, UI
construction prerequisites, re-entrancy prevention, timing constraints across
startup modes, or reverse index stability appear arbitrary or interchangeable
when inspected in isolation. Without explanatory comments, future maintainers
cannot distinguish intentional sequencing from arbitrary placement, risking
accidental regressions or subtle layout bugs when statements are reordered or
consolidated during refactoring. Furthermore, teardown comments that merely
describe immediate symptoms or local workarounds fail to communicate the
underlying architectural requirement; explicitly documenting the lifecycle
nesting invariant (such as reversing the creation or initialization sequence)
makes the contract clear and prevents maintainers from inadvertently violating
object lifetime boundaries. Conversely, arbitrarily reordering calls without
functional necessity introduces diff churn and prompts reviewers to wonder if
there is an unstated order dependency.

## Examples

Bad:

```cpp
// Pattern 1: Prerequisite setup ordering before presentation unannotated
[_inputPlateCoordinator start];
[_viewController addInputViewController:_inputPlateCoordinator.inputViewController];
[self.baseViewController presentViewController:_viewController
                                      animated:YES
                                    completion:nil];

// Pattern 2: Teardown comment describes symptom rather than lifecycle invariant
// Stop layout coordinator first to remove views while BVC is still valid.
[_browserLayoutCoordinator stop];
_browserLayoutCoordinator = nil;

// Pattern 3: Framework side-effect timing unannotated
[self takeToolbarSnapshots];
[_tabGridViewController.view addSubview:_browserLayoutViewController.view];

// Pattern 4: Gratuitous reordering when relative order does not matter
- (void)sceneViewDidMoveToWindow:(SceneView*)sceneView {
  [self updateAssistantLayout];
  [self updateLayoutForViews];
}
```

Good:

```cpp
// Pattern 1: Explains prerequisite setup ordering before presentation
[_inputPlateCoordinator start];
[_viewController addInputViewController:_inputPlateCoordinator.inputViewController];
// Present only AFTER the child view controller is added so the initial layout
// pass includes the input plate.
[self.baseViewController presentViewController:_viewController
                                      animated:YES
                                    completion:nil];

// Pattern 2: Teardown comment explicitly states the lifecycle invariant
// Stop layout coordinator first to follow the reverse order of start calls.
[_browserLayoutCoordinator stop];
_browserLayoutCoordinator = nil;

// Pattern 3: Documents framework side-effect timing dependency
// Take snapshots before adding subview because snapshotting forces a screen update.
[self takeToolbarSnapshots];
[_tabGridViewController.view addSubview:_browserLayoutViewController.view];

// Pattern 4: Preserves original call order to avoid diff churn
- (void)sceneViewDidMoveToWindow:(SceneView*)sceneView {
  [self updateLayoutForViews];
  [self updateAssistantLayout];
}
```
