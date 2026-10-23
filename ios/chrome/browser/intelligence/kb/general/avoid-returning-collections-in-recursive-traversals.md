---
name: ios/avoid-returning-collections-in-recursive-traversals
description: When collecting items during recursive tree or graph traversals, avoid returning and concatenating collections by value; accumulate in place via a reference or visitor
rubric: readability
severity: nit
---

**Rule.** Do not return and concatenate collections (such as `std::vector` or
`std::set`) by value at each level of a recursive tree, hierarchy, or graph
traversal. Instead, pass an accumulator container or visitor by non-const
reference (or pointer) and append or record elements in place.

**Why.** Returning a new collection by value at every level of recursion and
concatenating child results into the parent's container incurs O(N * depth) heap
allocations and element copies, whereas passing an accumulator container or
visitor by reference collects elements in place using a single buffer.

## Examples

Bad:

```cpp
// Bad: Returning a std::vector by value and concatenating at each recursive
// level causes O(N * depth) heap allocations and copies.
std::vector<FooNodeId> CollectSubtreeIds(const FooNode& node) {
  std::vector<FooNodeId> ids = {node.id()};
  for (const FooNode& child : node.children()) {
    std::vector<FooNodeId> child_ids = CollectSubtreeIds(child);
    ids.insert(ids.end(), child_ids.begin(), child_ids.end());
  }
  return ids;
}
```

Good:

```cpp
// Good: Pass an accumulator by reference to collect items in place.
void CollectSubtreeIds(const FooNode& node,
                       std::vector<FooNodeId>& accumulator) {
  accumulator.push_back(node.id());
  for (const FooNode& child : node.children()) {
    CollectSubtreeIds(child, accumulator);
  }
}
```
