---
name: ios/commands-strictly-for-ui-transitions-not-model-mutations
rubric: design
severity: should_fix
---

**Rule.** UI commands must be used exclusively to request UI changes (presenting
or dismissing UI). They must never be used to mutate model state, perform web
navigations, or alter data.

**Why.** Using command dispatch for model operations bypasses the model layer's
observation mechanics and creates hidden side effects. See
`ios/architecture/commands.md#characteristics-of-a-ui-command`.

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
