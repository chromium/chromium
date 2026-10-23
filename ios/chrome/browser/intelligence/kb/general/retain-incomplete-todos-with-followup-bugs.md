---
name: ios/retain-incomplete-todos-with-followup-bugs
description: When refactoring or implementing code touching TODOs, remove obsolete comments once resolved, but retain, relocate, or update descriptions for incomplete or partially addressed tasks
rubric: readability
severity: should_fix
citations:
  - path: docs/ios/style.md
    heading: TODO comments
    quote: "Sometimes we include TODO comments in code. Generally we follow [C++ style](https://google.github.io/styleguide/cppguide.html#TODO_Comments), but here are some more specific practices we've agreed upon as a team: * **Every TODO must have a bug**"
---

**Rule.** When completing tracked work, updating build dependencies, or adapting
logic where legacy constraints no longer apply, remove obsolete `TODO` comments
and associated workaround code. Conversely, do not delete, drop, or overwrite
`TODO` comments or strip their `TODO(<bug>)` references when copying,
refactoring, modernizing, or restructuring code while tasks or limitations
remain:

- **Relocation**: If surrounding code moves or changes structure, relocate and
  adapt the comment to the new call site or context rather than deleting it.
- **Partial completion**: When a preparatory change partially addresses a task
  (e.g., introducing prerequisite helpers or classes without full caller
  integration or lifetime guarantees), update the TODO description to specify
  the exact remaining work and invariants rather than removing it prematurely.
- **Bug tracking**: Every retained `TODO` must reference an open issue. If
  closing a parent issue with pending sub-tasks, migrate the TODO to an active
  follow-up bug ID (otherwise remove the TODO).

**Why.** Leaving obsolete or blindly copied TODO comments after implementing
described functionality or updating build dependencies misleads maintainers into
believing pending tasks remain and bloats code with dead workaround logic.
Conversely, deleting TODO comments, stripping their bug references, or failing
to relocate them during unrelated refactoring silently drops tracking of known
issues, behavioral discrepancies, and technical debt. Outdated TODO descriptions
after partial implementations can mislead reviewers into believing work is fully
completed; clarifying remaining scope and invariants ensures deferred
integration, follow-up tasks, and design rationale remain visible and traceable.

## Examples

Bad:

```cpp
// Variant 1: Dropping unresolved TODO during refactoring.
// [Refactoring removed TODO(crbug.com/492115056) without verifying fix]
[tabOpener dismissModalsAndMaybeOpenSelectedTabInMode:targetMode
                                    withUrlLoadParams:params];

// Variant 2: Retaining obsolete TODO after implementing the feature.
// TODO(crbug.com/480945289): Add searches for Labels (when enabled).
if (isRenderedInTopLayer(element)) { return true; }

// Variant 3: Leaving outdated text when prerequisite change only partially completes task.
// TODO(crbug.com/414585765): Create ScopedProfileKeepAlive mechanism.
void OnProfileLoaded(ScopedProfileKeepAliveIOS keep_alive) { ... }
```

Good:

```cpp
// Variant 1: Preserve and relocate unresolved TODO to new call site.
[tabOpener dismissModalsAndMaybeOpenSelectedTabInMode:targetMode
                                    withUrlLoadParams:params];
// TODO(crbug.com/492115056): Confirm toast display with enterprise policy.

// Variant 2: Remove obsolete TODO once the feature or dependency is updated.
if (isRenderedInTopLayer(element)) { return true; }

// Variant 3: Update description to specify remaining work and invariants.
// TODO(crbug.com/414585765): Ensure ScopedProfileKeepAliveIOS is kept alive
// until asynchronous callback and background processing complete.
void OnProfileLoaded(ScopedProfileKeepAliveIOS keep_alive) { ... }
```
