---
name: ios/command-methods-must-return-void
description: When declaring UI command protocol methods, return void; command methods are one-way dispatch instructions, not queries or synchronous RPCs.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Characteristics of a UI Command
    quote: Command methods must always return void. Commands are one-way requests, not queries.
---

**Rule.** All methods in command protocols must return `void`. UI commands are
one-way dispatch messages, not queries or request-response pairs.

**Why.** Returning values from commands couples callers to specific handler
implementations and assumes immediate synchronous availability across feature
boundaries. See `ios/architecture/commands.md#characteristics-of-a-ui-command`.

## Examples

Bad:

```cpp
@protocol SettingsCommands <NSObject>
- (BOOL)isSettingsUIPresented;
@end
```

Good:

```cpp
@protocol SettingsCommands <NSObject>
- (void)showSettingsUI;
- (void)hideSettingsUI;
@end
```
