---
name: ios/document-enums-and-enumerator-values
description: When documenting enums, enumerators, or state/event constants, describe behaviors and trigger mappings without tautology, qualify transition directions, and cite mirrored upstream or schema origins
rubric: readability
severity: should_fix
citations:
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
  - path: ios/web_view/README.md
    quote: Contains code to build an Objective-C framework which renders web content with `[CWVWebView]`. See the exposed API in `//ios/web_view/public/*` for more details.
---

**Rule.** Document enum declarations with comments describing what the type
controls and caller usage, avoiding tautological restatements of type names.
Document individual enumerators and related constant values (such as event
triggers, feature flags, or subsystem IDs) unless self-evident, explaining
concrete behaviors, what causes the event trigger, which feature it corresponds
to, non-obvious invariants, side effects, or domain acronyms rather than
redundant placeholders. When enumerators span multiple flows or distinct state
transitions, describe all valid contexts and explicitly qualify the transition
direction for each trigger action rather than relying on an ambiguous shared
verb, while scoping comments strictly to handled behaviors without speculative
caveats. Additionally, when an enum, enumerator, ID, or constant mirrors an
upstream framework type, external definition, Protocol Buffer schema, or server
protocol, explicitly cite the counterpart symbol name and the canonical origin
header or schema file using its full repository-relative path so cross-boundary
dependencies remain verifiable and synchronized.

**Why.** Tautological comments repeating type names add visual noise without
conveying context, while copy-pasted placeholders conceal behavioral
distinctions and non-obvious domain meanings. Explicitly qualifying transition
directions across multi-flow triggers prevents ambiguous verbs from
misrepresenting opposite state changes (such as entering versus exiting a mode),
while omitting speculative caveats keeps contracts focused on supported
behavior. Documenting what triggers an event or what underlying feature an entry
abstracts ensures maintainers understand the domain state. Citing the full
repository-relative path and counterpart symbol name for enums and constants
that mirror upstream framework types or protobuf schemas prevents values from
silently falling out of sync and makes cross-boundary dependencies traceable.

## Examples

Bad:

```cpp
// Bad: Tautological enum comment and unexplained feature mapping.
// Theme describing the look of the grid.
enum class GridTheme {
  kDynamic = 1,
  kDark,
};
enum class AIPrototypingFeature : NSInteger {
  kFreeform,
};

// Bad: Ambiguous transition direction across multiple actions.
// Reported when exiting fullscreen via toolbar or menu tap.
kForcedByUser = 3,

// Bad: Constant or enum mirroring an upstream type or proto schema without the origin path.
inline constexpr char kShortcutEntrypointAimID[] = "62";
enum class PasswordImportStatus {
  kNone,
  kUnknownError,
};
```

Good:

```cpp
// Good: Explains what the enum controls and what each feature represents.
// Specifies whether the grid matches the device appearance or forces dark mode.
enum class GridTheme {
  // Follows the system light or dark appearance.
  kDynamic = 1,
  // Always renders in dark appearance.
  kDark,
};
enum class AIPrototypingFeature : NSInteger {
  // Represents the `kAIPrototyping` feature used for model querying.
  kFreeform,
};

// Good: Qualifies transition direction for each trigger action.
// Reported when exiting fullscreen via toolbar tap or entering via menu tap.
kForcedByUser = 3,

// Good: Cites canonical repository-relative path and counterpart symbol name.
// Mirrored from IOS_CHROME_OMNIBOX_SEARCH_ENTRY_POINT in
// third_party/omnibox_proto/chrome_aim_entry_point.proto.
inline constexpr char kShortcutEntrypointAimID[] = "62";
// Matches password_manager::ImportEntry::Status in
// components/password_manager/core/browser/import/import_results.h.
enum class PasswordImportStatus {
  // Should not be used.
  kNone,
  // Any other error state.
  kUnknownError,
};
```
