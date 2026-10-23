---
name: ios/avoid-naive-xor-in-hash
description: When implementing -hash or hash functions combining small integers or enums, avoid raw XOR; shift fields into separate bit positions or use base::HashCombine
rubric: correctness
severity: should_fix
---

**Rule.** When implementing `-hash` (or a hash combination function) for types
with multiple integer, boolean, or enum fields, do not combine them using raw
bitwise XOR (`^`) without shifting. Instead, shift small-range fields into
distinct bit positions (e.g.,
`(static_cast<NSUInteger>(std::to_underlying(type)) << 31) + index`) or use a
dedicated hash combination helper such as `base::HashCombine`.

**Why.** Small integer and enum fields concentrate non-zero bits in their
lowest-order positions. Simply XORing multiple small-range or boolean fields
causes low bits to cancel and overlap, producing severe hash collisions (e.g.,
(0, 1) and (1, 0) both hash to 1). In hash tables, sets, and
`UITableViewDiffableDataSource` snapshots, high collision rates degrade lookup,
insertion, and diffing performance from O(1) toward O(N).

## Examples

Bad:

```cpp
- (NSUInteger)hash {
  // BAD: XORing low-range enum and integer causes immediate collisions (e.g. (0, 1) == (1, 0)).
  return static_cast<NSUInteger>(std::to_underlying(self.type)) ^ self.index;
}
```

Good:

```cpp
// GOOD: Combine fields with base::HashCombine or shift into non-overlapping bit ranges.
- (NSUInteger)hash {
  return base::HashCombine(0u, std::to_underlying(self.type), self.index);
}

// Also GOOD: Shift small-range fields into distinct bit positions.
- (NSUInteger)hash {
  return (static_cast<NSUInteger>(std::to_underlying(self.type)) << 31) +
         self.index;
}
```
