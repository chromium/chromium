---
name: ios/guard-enclosing-pointer-before-member-access
description: When accessing member fields on a nullable pointer, guard the enclosing pointer against null rather than checking embedded non-pointer value members
rubric: correctness
severity: should_fix
---

**Rule.** When accessing members of an object through a nullable pointer, guard
the enclosing pointer itself (`if (!ptr) return;` or `if (ptr)`) at the start of
the scope before any `ptr->member` access. Do not rely on testing a member field
(`if (ptr->member)`) to guard subsequent member accesses, as evaluating
`ptr->member` dereferences `ptr` immediately.

**Why.** Accessing `ptr->member` unconditionally dereferences `ptr` before the
condition can be evaluated, crashing with a null pointer dereference if `ptr` is
null. Guarding the enclosing pointer upfront ensures all subsequent member
accesses in the scope are safe.

## Examples

Bad:

```cpp
- (void)revertState {
  // Dereferences nullable _omniboxTextModel without guarding the pointer itself.
  if (_omniboxTextModel->user_input_in_progress) {
    _omniboxTextModel->input.Clear();
  }
  [self setInputInProgress:NO];
  _omniboxTextModel->paste_state = OmniboxPasteState::kNone;
}
```

Good:

```cpp
- (void)revertState {
  // Early return guards the enclosing pointer before any member access.
  if (!_omniboxTextModel) {
    return;
  }

  [self setInputInProgress:NO];
  _omniboxTextModel->input.Clear();
  _omniboxTextModel->paste_state = OmniboxPasteState::kNone;
}
```
