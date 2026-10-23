---
name: ios/match-header-filenames-to-defined-types
description: When defining or refactoring classes, interfaces, and headers, match snake_case filenames to primary type names in UpperCamelCase, avoiding truncated, stale, or misleading names
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: File names
    quote: Files should always be named using C++ style (`snake_case.mm`, not `CamelCase.mm`), even if they contain Objective-C++.
  - path: docs/ios/working_with_files.md
    heading: Moving and renaming files.
    quote: 'Renaming a file involves updating the filename in all of the places where it exists: the file names in the filesystem, the file names in git, header guards in files, import declarations in files, listings in BUILD.gn files, and internally in the Xcode project.'
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
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
