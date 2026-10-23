---
name: ios/defer-branch-specific-instantiation
description: When declaring variables, instantiating objects, or evaluating method calls conditionally, defer computation into consuming branches, inline checks, or place after early-return guards
rubric: safety
severity: should_fix
citations:
  - path: styleguide/c++/defining_compile_time_const.md
    heading: Guidelines
    quote: 1. **Define constants [in the narrowest scope possible](https://google.github.io/styleguide/cppguide.html#Local_Variables)**, just like with other variables.
  - path: base/memory/README.md
    heading: base::WeakPtr<T>
    quote: When the associated factory (usually owned by T) is destroyed, all `WeakPtr<T>` are invalidated (becomes null) rather than becoming use-after-frees.
---

**Rule.** Do not declare local variables, read timestamps, instantiate objects,
create closures, resolve handlers, or evaluate property lookups in an outer
scope when only needed within a specific conditional branch or after early
returns. Defer computations directly into consuming branches or after
early-return guards rather than executing them unconditionally upfront. In
conditional or early-return ladders, when a method call or condition check is
used once, or only on a later branch, inline it directly into its `if` guard
rather than eagerly precomputing it into an intermediate variable, preventing
unnecessary dynamic dispatch (`objc_msgSend`) on unexecuted paths. Conversely, a
property or method value that is read in more than one guard or branch of the
same method is stored once in a local variable and reused, rather than re-read
at each use. When conditionally assigning a variable, use `if`/`else` or a
ternary operator instead of overwriting a precomputed fallback. In constructors
and methods that return early on a nullable parameter, place the guard first and
assign dependent fields after it; never dereference such a parameter in a member
initializer list, which runs before the body's guard.

**Why.** Instantiating objects, resolving handlers, capturing blocks, reading
timestamps, evaluating fallback expressions, or executing method calls before
conditional checks or early returns wastes CPU cycles, allocates memory, and
adds unnecessary ARC/weak-reference overhead on unexecuted paths. In Objective-C
and Objective-C++, method calls use dynamic dispatch (`objc_msgSend`), which
cannot be reordered or eliminated by the compiler; eagerly evaluating them into
single-use variables forces redundant execution even when earlier branches match
and return immediately, while re-reading the same property in several guards
repeats that dispatch, so a value used more than once is cached in a local.

## Examples

Bad:

```cpp
// Fallback overwrite or hoisted branch allocation:
UIColor* color = tab_groups::ColorForTabGroupColorId(id);
if (flag) color = [[TabGroupColorPalette alloc] initWithSeedColorId:id].commonColor;

// Lookups or closures hoisted before early returns:
id<PriceCommands> handler = HandlerForProtocol(dispatcher, PriceCommands);
if (!showCompletion) return;
[handler showPriceSheet];

// Eager property/method evaluations before branch ladder:
BOOL isLarge = self.isInLargeDetent;
BOOL isMedium = self.isInMediumDetent;
NSString* identifier = _sheet.selectedDetentIdentifier;
if (isLarge) return SheetDimensionState::kLarge;
if (isMedium) return SheetDimensionState::kMedium;
if ([identifier isEqualToString:kPeakId]) return SheetDimensionState::kPeaking;
```

Good:

```cpp
// Fallback overwrite (use ternary or branch scoping):
UIColor* color = flag
    ? [[TabGroupColorPalette alloc] initWithSeedColorId:id].commonColor
    : tab_groups::ColorForTabGroupColorId(id);

// Computation or lookup deferred after early-return guard:
if (!showCompletion) return;
id<PriceCommands> handler = HandlerForProtocol(dispatcher, PriceCommands);
[handler showPriceSheet];

// Inlined single-use branch checks; the detent identifier is read once after
// the earlier returns and cached because two guards use it:
if ([self isInLargeDetent]) return SheetDimensionState::kLarge;
if ([self isInMediumDetent]) return SheetDimensionState::kMedium;
NSString* identifier = _sheet.selectedDetentIdentifier;
if ([identifier isEqualToString:kPeakId]) return SheetDimensionState::kPeaking;
if ([identifier isEqualToString:kHiddenId]) return SheetDimensionState::kHidden;
```
