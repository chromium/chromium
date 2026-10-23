---
name: ios/match-api-available-to-minimum-required-sdk-version
description: When gating iOS code with @available or API_AVAILABLE, annotate declarations but not symbol-free types, guard call sites, match SDK versions, and document bounded ranges
rubric: correctness
severity: should_fix
---

**Rule.** When introducing or gating functions, classes, methods, or properties
dependent on specific iOS versions (including test helpers), annotate
declarations with `API_AVAILABLE(ios(...))` and guard call sites with runtime
`@available(iOS X, *)` checks rather than checking availability internally.
Specify the exact minimum SDK version required by APIs used, avoiding
under-restrictive checks, over-restrictive versions copied from callers, or
comment mismatches. Do not annotate custom types (such as protocols,
`enum class` definitions, or block aliases) with `API_AVAILABLE` if their
definitions do not reference OS-restricted symbols. Additionally, when
documenting version workarounds or constants bounded by checks, state the full
affected version range (e.g., `iOS X.0 - iOS X.3`) and applicable device
conditions in comments.

**Why.** Specifying inaccurate versions in `@available` checks or omitting
`API_AVAILABLE` annotations causes code to execute on unsupported OS releases,
triggers runtime crashes, or creates confusing discrepancies with feature
specifications. Annotating custom type definitions with `API_AVAILABLE`
unnecessarily forces all referencing code to add availability annotations or
runtime checks even when no OS symbols are invoked. Furthermore, annotating
public declarations enforces requirements at compile time, while explicitly
stating full version ranges and device conditions in comments prevents
misleading maintainers about which point releases and devices execute a
workaround.

## Examples

Bad:

```cpp
// Bad: Unannotated version-dependent function or property and unguarded call sites.
bool MoveWindowToSide(int window_number, bool left_side);
@property(nonatomic, readonly) CWVFindInPageController* findInPageController;
[alertController addAction:[UIAlertAction actionWithTitle:@"Find" handler:^(UIAlertAction* a) {
  [weakSelf startFindInPageSession];
}]];

// Bad: Copies over-restrictive iOS 18.4 when APIs used only require iOS 17.4.
API_AVAILABLE(ios(18.4))
@interface ContextMenuPresenter : NSObject

// Bad: Under-restrictive check mismatches iOS 26 feature requirement and comment.
if (@available(iOS 19.0, *)) {
  options.placement = [UIWindowSceneProminentPlacement prominentPlacement];
}

// Bad: Uses internal @available checks instead of annotating OS-gated class.
- (void)startPrompt {
  if (@available(iOS 18.0, *)) {
    [ASSettingsHelper requestToTurnOn...];
  }
}

// Bad: Annotates custom protocol and block alias with API_AVAILABLE when definitions use no OS symbols.
API_AVAILABLE(ios(16.0))
@protocol FindInPageConsumer <NSObject>
- (void)setMatchCount:(NSInteger)matchCount;
@end
using FindInPageCompletion = void (^)(BOOL found) API_AVAILABLE(ios(16.0));

// Bad: Comment states only base release and omits device constraints on constant.
// Top padding for the header view on iOS 26.0.
constexpr CGFloat kIOS26HeaderViewTopPadding = 10;
if (@available(iOS 26.0, *)) {
  if (!@available(iOS 26.4, *)) {
    return kIOS26HeaderViewTopPadding;
  }
}
```

Good:

```cpp
// Good: Annotates OS-dependent functions and properties with API_AVAILABLE and guards call sites.
bool MoveWindowToSide(int window_number, bool left_side) API_AVAILABLE(ios(26.0));
// (In //ios/web_view, whose deployment target is lower than Chrome's.)
@property(nonatomic, readonly)
    CWVFindInPageController* findInPageController API_AVAILABLE(ios(16.0));
if (@available(iOS 16.0, *)) {
  [alertController addAction:[UIAlertAction actionWithTitle:@"Find" handler:^(UIAlertAction* a) {
    [weakSelf startFindInPageSession];
  }]];
}

// Good: Specifies the exact minimum SDK version required by APIs actually used
// (above the deployment target, otherwise the annotation is obsolete).
API_AVAILABLE(ios(18.4))
@interface ContextMenuPresenter : NSObject

// Good: Runtime check matches the exact target OS release for the API/feature.
if (@available(iOS 26.0, *)) {
  options.placement = [UIWindowSceneProminentPlacement prominentPlacement];
}

// Good: Annotates OS-gated coordinator at class level; callers guard entry points.
API_AVAILABLE(ios(26.0))
@interface MyCoordinator : ChromeCoordinator
@end

// Good: Leaves custom type definitions unrestricted across all OS versions.
@protocol FindInPageConsumer <NSObject>
- (void)setMatchCount:(NSInteger)matchCount;
@end
using FindInPageCompletion = void (^)(BOOL found);

// Good: Comment documents device conditions and full affected OS version range.
// Top padding for header view on iPhones with normal height and iOS 26.0 - 26.3.
constexpr CGFloat kIOS26HeaderViewTopPadding = 10;
if (@available(iOS 26.0, *)) {
  if (!@available(iOS 26.4, *)) {
    return kIOS26HeaderViewTopPadding;
  }
}
```
