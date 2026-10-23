---
name: ios/document-delegate-invocation-context-and-callback-obligation
description: When declaring Objective-C delegate or observer protocols, document component roles, trigger conditions, obligations, and completion contracts, and name callbacks after component properties
rubric: readability
severity: should_fix
citations:
  - path: docs/documentation_guidelines.md
    heading: Interface / class documentation
    quote: Non-trivial classes or sets of function declarations in header files should include comments with a brief description of the purpose of the class or set of functions as well as examples of expected usage.
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: agents/prompts/common.minimal.md
    heading: Coding
    quote: 'Add code comments sparingly: Focus on *why* something is done, not *what* is done.'
---

**Rule.** When declaring Objective-C delegate or observer protocols in header
files:

1. **Protocol declarations**: Precede `@protocol` declarations with a doc
   comment describing the component's general role (e.g.,
   `// Delegate protocol for <ClassName>.`) rather than narrowing it to a single
   feature or action that prompted its introduction.
2. **Callback naming and trigger conditions**: Name and document callback
   methods after the property or state of the observed/delegating component that
   changed (such as `zoomScale` or `frame`) rather than external caller triggers
   (such as `initial-scale`). Precede each callback method with a doc comment
   specifying the concrete trigger condition (e.g., `// Called when ...`)—such
   as distinguishing programmatic updates from user-driven interactions—rather
   than high-level caller features or presumed UI side effects. Avoid empty doc
   placeholders (`//`).
3. **Delegate obligations and completion contracts**: Explicitly document any
   delegate lifecycle responsibilities (such as dismissing UI, stopping child
   coordinators, or persisting state). For completion blocks, document execution
   timing, parameter semantics, cancellation behavior, and nil handling, in
   complete sentences with parameter names in backticks (never `|pipes|`).
4. **Edge cases**: For rare edge-case callbacks (such as external mutations or
   multi-window races), document why and how the scenario occurs.

**Why.** Delegate and observer protocols establish decoupled architectural
boundaries. Naming or documenting protocols and callbacks around external caller
triggers (such as viewport initial-scale) or presumed UI side effects leaks
caller implementation details into the component's public interface,
misrepresents what state actually changed, and couples the interface prematurely
as features evolve. Documenting specific component-domain state changes, trigger
conditions (including programmatic vs. user interactions), delegate lifecycle
obligations (such as UI dismissal and coordinator teardown), and completion
block contracts ensures implementers fulfill necessary duties without guessing,
preventing hung asynchronous flows, dead-code confusion, and modularity
violations.

## Examples

Bad:

```cpp
// Protocol doc narrowed to single feature; method doc assumes UI side effect:
// Delegate to handle the group edition request.
@protocol TabGroupHeaderDelegate <NSObject>
// Shows the group's edition menu.
- (void)tabGroupHeaderDidTapTitle:(TabGroupHeader*)tabGroupHeader;
@end

// Named and documented after external caller trigger rather than component state:
// Called when the initial-scale is changed.
- (void)webViewScrollViewDidInitialScaleChange:
    (CRWWebViewScrollViewProxy*)scrollView;

// Missing trigger condition, teardown obligation, and completion contract:
- (void)coordinatorDidFinish:(AutofillCoordinator*)coordinator;
- (void)showCardOptions:(NSArray<CWVCardOption*>*)options
            acceptBlock:(void (^)(NSString* option))acceptBlock;
```

Good:

```cpp
// Protocol doc describes component role; method doc describes trigger event:
// Delegate protocol for TabGroupHeader.
@protocol TabGroupHeaderDelegate <NSObject>
// Called when the tab group header's title is tapped.
- (void)tabGroupHeaderDidTapTitle:(TabGroupHeader*)tabGroupHeader;
@end

// Named and documented after component state and programmatic invocation:
// Called when the zoom scale is changed outside of an actual user zoom action.
- (void)webViewScrollViewDidZoomScaleChange:
    (CRWWebViewScrollViewProxy*)scrollView;

// Documents trigger condition, delegate teardown duty, and parameter semantics:
// Called when the user acts in the save UI and coordinator should be dismissed.
- (void)coordinatorDidFinish:(AutofillCoordinator*)coordinator;
// Shows the card options. Invokes `acceptBlock` with the chosen option ID;
// passing nil aborts the flow.
- (void)showCardOptions:(NSArray<CWVCardOption*>*)options
            acceptBlock:(void (^)(NSString* selectedOptionId))acceptBlock;
```
