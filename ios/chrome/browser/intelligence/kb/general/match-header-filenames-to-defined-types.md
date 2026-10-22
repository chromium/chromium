---
name: ios/match-header-filenames-to-defined-types
rubric: readability
severity: should_fix
---

**Rule.** Ensure source and header filenames correspond 1:1 with the primary
class or interface defined within them, matching `snake_case` filenames to
`UpperCamelCase` type names (e.g., `OmniboxPopupRowTrailingButton` in
`omnibox_popup_row_trailing_button.h`). Avoid truncated or mismatched names that
omit component words from the filename or prefixes from the interface name.
Conversely, ensure filenames accurately reflect the types or concepts defined
(or use `*_types.{h,mm}` for consolidated declarations), avoiding one-sided or
misleading names (such as `*_entrypoint.h` when defining general reasons), and
rename or delete files when refactoring away or renaming their namesake types.

**Why.** Inconsistent or truncated class and interface names violate codebase
naming conventions and make symbols difficult to discover and navigate.
Mismatched, misleading, or stale filenames misrepresent component
responsibilities and obscure file contents in code search and build targets.

## Examples

Bad:

```cpp
// In ios/chrome/.../enterprise_groups_handler_factory.h
// Bad: Filename omits "profile" from the primary class name.
class EnterpriseGroupsProfileHandlerFactory
    : public ProfileKeyedServiceFactoryIOS {};

// In ios/chrome/.../omnibox_popup_row_trailing_button.h
// Bad: Primary interface name is truncated and does not match the filename.
@interface TrailingPopupRowButton : ExtendedTouchTargetButton
@end

// In ios/chrome/.../fullscreen_entrypoint.h
// Bad: Filename implies entry points only, but declares exit reasons.
enum class FullscreenExitReason { kUserTapped, kBottomReached };

// In ios/chrome/.../actor_tool_error.mm
// Bad: Stale filename retained after ActorToolError was replaced.
ToolExecutionResult::ToolExecutionResult(...) { ... }
```

Good:

```cpp
// In ios/chrome/.../enterprise_groups_profile_handler_factory.h
// Good: Filename matches the full class name in snake_case.
class EnterpriseGroupsProfileHandlerFactory
    : public ProfileKeyedServiceFactoryIOS {};

// In ios/chrome/.../omnibox_popup_row_trailing_button.h
// Good: Primary interface name matches the snake_case filename.
@interface OmniboxPopupRowTrailingButton : ExtendedTouchTargetButton
@end

// In ios/chrome/.../fullscreen_reason.h
// Good: Filename accurately reflects the reason enum defined inside.
enum class FullscreenExitReason { kUserTapped, kBottomReached };

// In ios/chrome/.../actor_tool_types.mm
// Good: Renamed to actor_tool_types.{h,mm} to match consolidated types.
ToolExecutionResult::ToolExecutionResult(...) { ... }
```
