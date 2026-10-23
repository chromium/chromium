---
name: ios/name-condition-enums-as-state-not-availability
description: Suffix enums and state properties representing conditions, lifecycles, or operation outcomes with State, Status, Result, or Outcome rather than bare nouns or Availability
rubric: readability
severity: should_fix
---

**Rule.** When defining an enum type (normally an `enum class`) that represents
an entity's condition, mode, lifecycle, classification, or the outcome of an
operation or process, suffix the type name with an explicit descriptor such as
`State`, `Status`, `Result`, or `Outcome` (e.g., `DefaultSearchEngineState`,
`SharingState`, `PasskeysMigrationStatus`). Do not name the enum using a bare
entity noun (which misleadingly suggests an entity model or object instance), a
bare action or process noun (which reads like an actor, task, or runner class),
or `Availability` (unless strictly modeling access readiness or feature
toggles). Additionally, when declaring properties or member variables that store
state enums, append `State` or `Status` to the property name (e.g.,
`currentDefaultState`), and avoid comments that describe the field as a domain
entity object; refer directly to the enum definition (e.g.,
`// See \`SearchEngineCurrentDefaultState\`.\`) or accurately describe the
represented state.

**Why.** Naming an enum or state property after a bare entity suggests that the
type or variable represents a domain model object or pointer rather than a
scalar condition. Similarly, naming an outcome enum after a bare action or
process noun (such as `PasskeysMigration`) suggests a service, runner, or actor
class performing the work rather than an outcome value type, and using
`Availability` misleadingly implies access readiness or feature gating instead
of a mode or lifecycle stage. Explicit suffixes such as `State`, `Status`,
`Result`, or `Outcome` clarify the type's purpose in header declarations, call
sites, and telemetry logging.

## Examples

Bad:

```cpp
// Misleading bare entity noun implies model instance; Availability implies feature toggle.
enum class DefaultSearchEngine { kGoogle, kNotGoogle };
enum class ShareAvailability { kNotShared, kShared };

// Bare action noun reads like an actor or task class rather than an outcome.
enum class PasskeysMigration { kPasskeyCreated, kPasskeyUpdated };

// Bare entity property name and comment mislead readers into expecting an object pointer.
// Current default search engine, see `SearchEngineCurrentDefault`.
@property(nonatomic, assign) SearchEngineCurrentDefault currentDefault;
```

Good:

```cpp
// Explicit 'State' suffix communicates entity condition or mode.
enum class DefaultSearchEngineState { kGoogle, kNotGoogle };
enum class SharingState { kNotShared, kShared };

// Explicit 'Status' suffix clarifies operation outcome.
enum class PasskeysMigrationStatus { kPasskeyCreated, kPasskeyUpdated };

// Property and comment explicitly indicate scalar state enum.
// See `SearchEngineCurrentDefaultState`.
@property(nonatomic, assign) SearchEngineCurrentDefaultState currentDefaultState;
```
