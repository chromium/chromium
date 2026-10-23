---
name: ios/extract-nested-conditionals-to-helpers
description: When orchestrators or UIKit and gesture delegates contain complex conditionals, telemetry, or coordinate algorithms, extract dedicated helpers or standalone testable units
rubric: design
severity: should_fix
citations:
  - path: styleguide/c++/c++.md
    heading: Unnamed namespaces
    quote: Items local to a .cc file should be wrapped in an unnamed namespace.
  - path: styleguide/objective-c/objective-c.md
    heading: Delineate interface implementations with `#pragma mark -`
    quote: Private methods can be grouped differently if it helps make the code more readable; for example, private methods that are just helpers for methods in specific protocols could be grouped under a `#pragma mark - <Protocol> helpers`, directly after the protocol methods, instead of with the other private methods.
  - path: docs/ios/testing.md
    heading: Before you start
    quote: Just write a unit test if the purpose of your test does not involve UI.
---

**Rule.** Extract multi-step subtasks, nested conditionals, complex switch
statements, multi-step heuristics, and secondary concerns (such as metrics,
telemetry logging, and feature-guarded reporting) from caller, orchestration,
dispatching, and UI or gesture delegate methods (such as
`tableView:didSelectRowAtIndexPath:`,
`collectionView:dropSessionDidUpdate:withDestinationIndexPath:`, or
`gestureRecognizer:shouldRecognizeSimultaneouslyWithGestureRecognizer:`) into
dedicated private helper methods or unnamed-namespace functions local to the
implementation file. Within Objective-C/Objective-C++ delegate classes, group
private protocol helpers under `#pragma mark - <Protocol> helpers` per style
guide, and use guard clauses with early returns to flatten nested branches and
view hierarchy inspections. Keep orchestrators and delegate callbacks focused on
high-level control flow, gesture lifecycle, or proposal returns. Additionally,
when decision logic involves coordinate thresholds, drag-and-drop boundary
checks, or geometric algorithms with multiple permutations, encapsulate the
algorithm into standalone helper classes, structs, or pure utility functions
decoupled from UIKit view hierarchies, and verify edge cases with hermetic unit
tests (`*_unittest.mm`) rather than slow EarlGrey integration tests
(`*_egtest.mm`).

**Why.** Inlining multi-level conditionals, complex switch statements,
coordinate threshold math, or secondary telemetry recording inside
orchestration, dispatcher, or delegate methods bloats method size, obscures
primary control flow, and tightly couples domain workflows or analytics to
high-level coordination. Delegating subtasks, branch arms, and domain condition
checks to self-documenting private helper methods or unnamed-namespace functions
isolates dependencies, clarifies decision sequences, resets indentation via
early returns, and enables logic reuse across sibling gesture or event handlers.
Furthermore, embedding coordinate and threshold algorithms directly in view
controllers binds logic to UIKit view hierarchies, preventing isolated unit
testing and forcing reliance on slow, flaky EarlGrey UI tests; extracting pure
helper functions or classes enables fast, deterministic unit test coverage.

## Examples

Bad:

```cpp
// Bad: Inlining secondary telemetry / feature checks in an orchestrator.
void Client::Proceed(ProfileIOS* profile) {
  if (profile && base::FeatureList::IsEnabled(kReport)) {
    GetRouter(profile)->Report(url_);
  }
  Reload();
}

// Bad: Inlining multi-step gesture heuristics directly in a delegate callback.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gr
    shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer*)other {
  BOOL isPan = [other isKindOfClass:[UIPanGestureRecognizer class]];
  BOOL isScroll = [other.view isKindOfClass:[UIScrollView class]];
  return !(isPan && isScroll && [self viewDescendantOfWKWebView:other.view]);
}

// Bad: Inlining coordinate threshold algorithms in UIKit delegates; relies on slow EarlGrey tests.
- (UICollectionViewDropProposal*)collectionView:(UICollectionView*)cv
                           dropSessionDidUpdate:(id<UIDropSession>)session
                       withDestinationIndexPath:(NSIndexPath*)indexPath {
  CGRect cellFrame = [cv layoutAttributesForItemAtIndexPath:indexPath].frame;
  BOOL shouldGroup = (entryDirection == DragEntrySideLeft)
                         ? (location.x <= cellFrame.origin.x + cellFrame.size.width * 0.75)
                         : YES;
  return [[UICollectionViewDropProposal alloc] initWithDropOperation:UIDropOperationMove];
}
```

Good:

```cpp
// Good: Extract secondary concerns into anonymous-namespace or private helpers.
namespace {
void MaybeReport(ProfileIOS* profile, const GURL& url) {
  if (profile && base::FeatureList::IsEnabled(kReport)) {
    GetRouter(profile)->Report(url);
  }
}
}  // namespace

// Good: Delegate condition checks to focused helpers under pragma mark.
#pragma mark - UIGestureRecognizerDelegate helpers
- (BOOL)handlesInnerWKWebViewScrolls:(UIGestureRecognizer*)other {
  return [other.view isKindOfClass:[UIScrollView class]] &&
         [self viewDescendantOfWKWebView:other.view];
}

// Good: Encapsulate coordinate/threshold algorithms into pure units for hermetic *_unittest.mm.
- (UICollectionViewDropProposal*)collectionView:(UICollectionView*)cv
                           dropSessionDidUpdate:(id<UIDropSession>)session
                       withDestinationIndexPath:(NSIndexPath*)indexPath {
  CGRect cellFrame = [cv layoutAttributesForItemAtIndexPath:indexPath].frame;
  BOOL shouldGroup = [DragReorderClassifier shouldGroupAtLocation:location
                                                        cellFrame:cellFrame
                                                   entryDirection:entryDirection];
  return [[UICollectionViewDropProposal alloc] initWithDropOperation:UIDropOperationMove];
}
```
