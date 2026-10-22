---
name: ios/commands-pass-minimal-context-not-ui-or-geometry
rubric: design
severity: should_fix
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
