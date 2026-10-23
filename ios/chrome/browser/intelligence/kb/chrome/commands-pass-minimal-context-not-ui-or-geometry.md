---
name: ios/commands-pass-minimal-context-not-ui-or-geometry
description: When defining command parameters, do not pass UI-layer views or screen coordinates unless acting as an opaque presentation anchor.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Characteristics of a UI Command
    quote: Commands must not pass UI-layer objects (UIView*, UIViewController*) or screen geometry (CGPoint, CGRect) that dictate how or where the feature is presented relative to the rest of the UI. The one exception is a presentation anchor.
---

**Rule.** Commands must not take `UIView*`, `UIViewController*`, `CGRect`, or
`CGPoint` parameters, except when passed as an opaque presentation anchor for
iPad popovers.

**Why.** Passing UI geometry or views couples the command sender and receiver to
specific layout assumptions and presentation implementations. See
`ios/architecture/commands.md#characteristics-of-a-ui-command`.

## Examples

Bad:

```cpp
@protocol ShareCommands <NSObject>
- (void)showShareSheetForURL:(const GURL&)URL inViewController:(UIViewController*)viewController atPoint:(CGPoint)point;
@end
```

Good:

```cpp
@protocol ShareCommands <NSObject>
- (void)showShareSheetForURL:(const GURL&)URL anchorView:(UIView*)anchorView;
@end
```
