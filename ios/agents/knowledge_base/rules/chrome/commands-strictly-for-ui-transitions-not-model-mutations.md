---
name: ios/commands-strictly-for-ui-transitions-not-model-mutations
description: When using UI commands, use them exclusively to request UI changes; never use commands to mutate model state or perform web navigations.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Characteristics of a UI Command
    quote: Commands are exclusively for inducing UI changes (typically presenting or dismissing a UI feature). Operations that mutate model state—including core web navigation actions such as loading a URL, navigating back/forward, or mutating a `WebStateList`—must not use UI commands.
---

**Rule.** UI commands must be used exclusively to request UI changes (presenting
or dismissing UI). They must never be used to mutate model state, perform web
navigations, or alter data.

**Why.** UI commands form the UI layer's public API for requesting UI changes.
Code that needs to perform model mutations should directly depend on the
appropriate model services, and use them to directly make the necessary updates.
See `ios/architecture/commands.md#characteristics-of-a-ui-command`.

## Examples

Bad:

```cpp
@protocol NavigationCommands <NSObject>
- (void)loadURL:(const GURL&)URL;
- (void)deleteHistoryEntry:(int64_t)id;
@end
```

Good:

```cpp
@protocol NavigationCommands <NSObject>
- (void)showNavigationHistoryMenu;
@end
```
