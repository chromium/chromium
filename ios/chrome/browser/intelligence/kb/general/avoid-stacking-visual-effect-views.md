---
name: ios/avoid-stacking-visual-effect-views
description: When increasing blur opacity or density in UIKit views, configure a single UIVisualEffectView with thick material styles instead of stacking multiple visual effect views
rubric: design
severity: nit
---

**Rule.** Do not stack multiple `UIVisualEffectView` instances to increase blur
density or opacity. Configure a single `UIVisualEffectView` using an appropriate
UIKit material blur style (such as `UIBlurEffectStyleSystemThickMaterial`,
`UIBlurEffectStyleSystemThickMaterialDark`, or
`UIBlurEffectStyleSystemChromeMaterial`) to achieve the required visual depth.

**Why.** Layering multiple `UIVisualEffectView` instances forces UIKit and the
GPU to perform separate backdrop capture, filtering, and blending passes for
each layer. This incurs substantial rendering and compositing overhead, causing
dropped frames and increased power consumption. UIKit material styles natively
adjust blur radius and saturation within a single rendering pass.

## Examples

Bad:

```cpp
// Stacking multiple blur views multiplies GPU compositing passes.
for (int i = 0; i < kBlurLayersCount; ++i) {
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
  UIVisualEffectView* blurView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  [self addSubview:blurView];
}
```

Good:

```cpp
// Use a single visual effect view with a thicker material style.
UIBlurEffect* blurEffect = [UIBlurEffect
    effectWithStyle:UIBlurEffectStyleSystemThickMaterialDark];
UIVisualEffectView* blurView =
    [[UIVisualEffectView alloc] initWithEffect:blurEffect];
[self addSubview:blurView];
```
