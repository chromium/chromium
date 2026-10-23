---
name: ios/clear-container-adapters-by-reassignment
description: When clearing container adapters like std::queue, base::queue, or std::stack, reassign a default-constructed instance or empty braces instead of popping in a loop
rubric: readability
severity: nit
---

**Rule.** Do not clear container adapters such as `std::queue`, `base::queue`,
or `std::stack` by popping elements one by one in a
`while (!c.empty()) c.pop();` loop. Clear the container by reassigning a
default-constructed instance (such as `c = {};` or `c = decltype(c)();` /
`c = ContainerType();`) instead.

**Why.** Container adapters like `std::queue` and `std::stack` do not provide a
`.clear()` member function. Repeatedly popping elements in a while loop incurs
per-element loop condition checks and individual pop overhead, whereas
reassigning a default-constructed instance destroys all elements and releases
underlying container storage in a single bulk operation.

## Examples

Bad:

```cpp
// Bad: clearing container adapters element-by-element in a while loop
while (!task_queue.empty()) {
  task_queue.pop();
}

while (!eval_stack.empty()) {
  eval_stack.pop();
}
```

Good:

```cpp
// Good: reassign empty braces or a default-constructed instance
task_queue = {};

eval_stack = std::stack<Expression>();
```
