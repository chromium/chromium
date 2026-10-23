---
name: ios/prefer-last-or-first-object-over-boundary-subscript
description: When accessing boundary elements of an NSArray, prefer .lastObject or .firstObject over subscripting array[count - 1] or array[0] to avoid out-of-bounds crashes on empty containers
rubric: correctness
severity: should_fix
---

**Rule.** Prefer `array.lastObject` or `array.firstObject` over subscripting
`array[count - 1]` or `array[0]` when accessing boundary elements of an
`NSArray`, unless the container is already guaranteed to be non-empty. Avoid
direct index subscripting or `objectAtIndex:` on boundary indices when the
container may be empty to prevent unsigned `NSUInteger` underflow (`count - 1`
wrapping to `NSUIntegerMax`) and out-of-bounds `NSRangeException` crashes.

**Why.** In Cocoa, `-[NSArray lastObject]` and `-[NSArray firstObject]` safely
return `nil` on empty arrays, which can be safely messaged or evaluated in
Objective-C. In contrast, evaluating `count - 1` on an empty array underflows
unsigned `NSUInteger` to `NSUIntegerMax`, and subscripting `array[count - 1]` or
`array[0]` raises an unhandled `NSRangeException` runtime crash.

## Examples

Bad:

```cpp
// Unsigned underflow on empty array causes out-of-bounds crash:
NSUInteger count = _items.count;
if (!_items[count - 1].isPinned || count < kMaximumItems) {
  [indices addObject:@(kPlusButtonIdentifier)];
}

Item* firstItem = _items[0];
[firstItem performAction];
```

Good:

```cpp
// Safely evaluates to nil on empty arrays without crashing:
NSUInteger count = _items.count;
if (!_items.lastObject.isPinned || count < kMaximumItems) {
  [indices addObject:@(kPlusButtonIdentifier)];
}

Item* firstItem = _items.firstObject;
[firstItem performAction];
```
