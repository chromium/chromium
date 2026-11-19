---
name: ios/command-protocols-flat-no-protocol-inheritance
description: When declaring command protocols, do not inherit from other command protocols; keep command protocols flat and focused on a single domain.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Do Not Inherit or Compose Command Protocols
    quote: Command protocols must not inherit from other command protocols, and callers must not combine multiple command protocols into a single handler property or create synthetic composed protocols for handler dependencies.
  - path: ios/architecture/commands.md
    heading: Characteristics of a UI Command
    quote: Command protocols must be flat (`@protocol FooCommands` or `@protocol FooCommands <NSObject>`); a command protocol must never inherit from another command protocol.
---

**Rule.** Command protocols must be flat and must not inherit from other command
protocols (`@protocol FooCommands` or `@protocol FooCommands <NSObject>`).

**Why.** Each command protocol represents a single functional domain registered
independently with `CommandDispatcher`. Keeping protocols flat ensures callers
declare a separate, narrow handler dependency for each domain they use. See
`ios/architecture/commands.md#do-not-inherit-or-compose-command-protocols`.

## Examples

Bad:

```cpp
@protocol ExtendedSettingsCommands <SettingsCommands>
- (void)showAdvancedSettings;
@end
```

Good:

```cpp
@protocol AdvancedSettingsCommands <NSObject>
- (void)showAdvancedSettings;
@end
```
