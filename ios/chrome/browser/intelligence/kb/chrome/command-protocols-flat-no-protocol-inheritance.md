---
name: ios/command-protocols-flat-no-protocol-inheritance
description: When declaring command protocols, do not inherit from other command protocols; keep command protocols flat and focused on a single domain.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Characteristics of a UI Command
    quote: Command protocols must be flat (@protocol FooCommands or @protocol FooCommands <NSObject>); a command protocol must never inherit from another command protocol.
---

**Rule.** Command protocols must be flat and must not inherit from other command
protocols (i.e. use `@protocol FooCommands` or
`@protocol FooCommands <NSObject>`).

**Why.** Command dispatcher registers handlers per protocol; inheriting
protocols creates ambiguities in selector routing and target registration. See
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
