---
name: ios/consolidate-repeated-constants-in-anonymous-namespace
description: Extract magic numbers, UI layout dimensions, durations, thresholds, and repeated test parameters into named constexpr constants in an anonymous namespace, using distinct constants per semantic role
rubric: readability
severity: should_fix
citations:
  - path: styleguide/c++/defining_compile_time_const.md
    heading: Guidelines
    quote: Use `constexpr` inside an anonymous namespace for file-scope constants in `.cc` files.
  - path: docs/ui/learn/bestpractices/layout.md
    heading: Obtain layout values from provider objects
    quote: Code should not have any magic numbers for sizes, positions, elevations, and the like; this includes transformations like scaling values up or down by a fraction, or tweaking a provided value by a few DIPs.
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: For C functions and constants defined in a namespace, use C++ style, even if most of the file is Objective-C.
  - path: ios/best_practices.md
    heading: Language Choice
    quote: In `//ios/`, "Objective-C" always means Objective-C++ (`.mm`), which allows freely mixing Objective-C and C++. Plain Objective-C (`.m`) is not used outside `//ios/third_party/`.
---

**Rule.** Do not embed raw magic numbers, fractional multipliers, or inline
duration literals directly in method logic (UI layout dimensions, insets,
spacing, Auto Layout constraint constants, corner radii, transforms, alpha
values, symbol sizes, line counts, timeouts, cooldowns, animation durations),
declare fixed configuration thresholds or margins as mutable local variables
within method bodies, duplicate identical local constants across multiple
functions or test cases, or store fixed implementation-internal constants as
instance member variables. Reuse an existing component or subsystem constant
when one expresses the same concept (such as `kGridCellCornerRadius` or shared
layout tokens); otherwise extract the value into a descriptive named
`kCamelCase` compile-time constant (`constexpr`, or `const` for Objective-C
object types), placing shared constants and file-level configuration parameters
in an unnamed (anonymous) namespace at file scope unless they must be configured
per-instance or injected for testing, while keeping function-specific single-use
constants scoped locally. Declare separate, dedicated constants for distinct
layout roles or domain concepts (margins, padding, spacing) even when their
values happen to coincide, rather than sharing one generic constant across
unrelated concepts. When a constant is intentionally derived from another (such
as coupling an animation translation offset to layout padding), when a
constraint uses a calculated ratio or divisor, or when a view's
`-intrinsicContentSize` or `-sizeThatFits:` depends on a dimension constant of a
specific subview, variant, or animation, add a comment documenting the
intentional relationship and the UI assumption behind it.

**Why.** Hardcoded magic numbers, mutable local configuration variables, and
unnecessary instance member variables obscure semantic meaning, coordinate
units, and spatial or motion relationships, bloat object layout size, and
mislead readers into expecting per-instance configurability or mutation.
Defining descriptive named compile-time constants in an anonymous namespace
establishes a single source of truth, avoids per-instance storage overhead,
guarantees internal linkage within the translation unit, and lets tuning updates
propagate cleanly across callers. Reusing a single generic constant across
distinct UI concepts tightly couples independent elements so that adjusting one
silently breaks another, while documenting intentional derivations and
cross-subview sizing assumptions explains why values are linked and prevents
regressions during refactoring.

## Examples

Bad:

```cpp
// Variant 1: Duplicating identical local constants across multiple functions or tests.
TEST_F(IconUtilsTest, BrandedSymbolWithDifferentSizes) {
  const CGFloat size1 = 10.0;
  const CGFloat size2 = 20.0;
  UIImage* image1 = [IconUtils symbolWithPointSize:size1];
  ...
}

// Variant 2: Embedding inline magic numbers or duration literals directly in logic.
- (BOOL)inCooldown {
  return base::Time::Now() - _cooldownStartTs <= base::Microseconds(500);
}
_mainStackView.spacing = 16.0;
_iconContainer.layer.cornerRadius = 12.0;
[UIView animateWithDuration:0.3
                 animations:^{
                   [weakSelf fadeOutContent];
                 }];

// Variant 3: Declaring fixed configuration thresholds or margins as mutable local variables.
- (void)updateDropSession {
  CGFloat reorderMargin = 0.2;
  ...
  CGFloat threshold = cellFrame.size.width * (1.0 - reorderMargin);
}

// Variant 4: Storing fixed implementation-internal constants as instance member variables.
class FeatureTool {
 private:
  base::TimeDelta timeout_;
};
FeatureTool::FeatureTool() : timeout_(base::Seconds(1)) {}

// Variant 5: Sharing one generic constant across distinct roles; undocumented
// derivations and divisors.
constexpr CGFloat kSpacing = 16.0;
[view1.topAnchor constraintEqualToAnchor:superview.topAnchor constant:kSpacing];
[view2.leadingAnchor constraintEqualToAnchor:superview.leadingAnchor constant:kSpacing];
CGAffineTransformMakeTranslation(0.0, kToolbarPadding * (1.0 - progress));
constraint.constant = containerWidth / 3;
```

Good:

```cpp
namespace {
// Variant 1: Consolidate shared test constants in an anonymous namespace.
constexpr CGFloat kSmallPointSize = 10.0;
constexpr CGFloat kLargePointSize = 20.0;

// Variant 2: Extract file-level configuration values into named constants.
// Duration of the cooldown period during which user interactions are ignored.
constexpr base::TimeDelta kCooldownDuration = base::Microseconds(500);
constexpr CGFloat kMainStackSpacing = 16.0;
constexpr CGFloat kHeaderIconCornerRadius = 12.0;
constexpr NSTimeInterval kFadeAnimationDuration = 0.3;

// Variant 3: Define fixed configuration thresholds and margins as named constants.
constexpr CGFloat kDragReorderMargin = 0.2;

// Variant 4: File-internal constants belong at file scope, not in class member layout.
constexpr base::TimeDelta kTimeout = base::Seconds(1);

// Variant 5: Dedicated constants per layout role; documented derivations.
constexpr CGFloat kHeaderTopMargin = 16.0;
constexpr CGFloat kContentLeadingPadding = 16.0;
// Slide the location bar exactly to the toolbar edge.
constexpr CGFloat kNTPLocationBarTranslation = kToolbarPadding;
constexpr NSInteger kItemColumnsCount = 3;
}  // namespace

TEST_F(IconUtilsTest, BrandedSymbolWithDifferentSizes) {
  UIImage* image1 = [IconUtils symbolWithPointSize:kSmallPointSize];
  ...
}

- (BOOL)inCooldown {
  return _cooldownTimer.has_value() &&
         _cooldownTimer->Elapsed() <= kCooldownDuration;
}
_mainStackView.spacing = kMainStackSpacing;
_iconContainer.layer.cornerRadius = kHeaderIconCornerRadius;
[UIView animateWithDuration:kFadeAnimationDuration
                 animations:^{
                   [weakSelf fadeOutContent];
                 }];

- (void)updateDropSession {
  ...
  CGFloat threshold = cellFrame.size.width * (1.0 - kDragReorderMargin);
}

[view1.topAnchor constraintEqualToAnchor:superview.topAnchor constant:kHeaderTopMargin];
[view2.leadingAnchor constraintEqualToAnchor:superview.leadingAnchor constant:kContentLeadingPadding];
CGAffineTransformMakeTranslation(0.0, kNTPLocationBarTranslation * (1.0 - progress));
// Distribute spacing equally across columns.
constraint.constant = containerWidth / kItemColumnsCount;
```
