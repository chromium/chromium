---
name: ios/cache-bounds-for-path-and-mask-updates
description: When updating paths, shape layers, or layer masks during view layout passes, cache previous bounds and return early if CGRectEqualToRect indicates bounds are unchanged
rubric: readability
severity: nit
---

**Rule.** When updating paths, shape layers, or layer masks during view layout
passes (such as in or from `-layoutSubviews`), cache the previous bounds in an
ivar and return early if `CGRectEqualToRect(bounds, _lastBounds)` indicates the
geometry has not changed. Do not reallocate `UIBezierPath` or `CGPath` objects
or reassign layer masks on every layout pass when the view dimensions remain
identical.

**Why.** `-layoutSubviews` is called frequently during scrolling, animations,
gestures, and view hierarchy updates. Recreating bezier paths and reassigning
layer masks on every layout pass when bounds have not changed introduces
unnecessary heap allocations, CoreGraphics path calculations, and rendering
churn.

## Examples

Bad:

```cpp
- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateMask];
}

- (void)updateMask {
  CGRect bounds = self.bounds;
  if (CGRectIsEmpty(bounds)) {
    return;
  }
  // Recreated on every layout pass even when bounds haven't changed:
  _maskPath = [UIBezierPath bezierPathWithRoundedRect:bounds
                                         cornerRadius:kCornerRadius];
  _maskLayer.path = _maskPath.CGPath;
}
```

Good:

```cpp
- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateMask];
}

- (void)updateMask {
  CGRect bounds = self.bounds;
  if (CGRectIsEmpty(bounds) || CGRectEqualToRect(bounds, _lastBounds)) {
    return;
  }
  _lastBounds = bounds;
  _maskPath = [UIBezierPath bezierPathWithRoundedRect:bounds
                                         cornerRadius:kCornerRadius];
  _maskLayer.path = _maskPath.CGPath;
}
```
