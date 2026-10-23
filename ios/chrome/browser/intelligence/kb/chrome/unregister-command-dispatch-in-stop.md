---
name: ios/unregister-command-dispatch-in-stop
description: When a coordinator registers as a command dispatcher target in start, unregister it using stopDispatchingToTarget in stop.
rubric: safety
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Configuring and Stopping Dispatch
    quote: When the handling coordinator stops, unregister it from the dispatcher.
---

**Rule.** When a coordinator registers a target for command protocols on its
`Browser`'s `CommandDispatcher` in `-start` (with
`-startDispatchingToTarget:forProtocol:` on
`self.browser->GetCommandDispatcher()`), it must unregister that target in
`-stop` with `-stopDispatchingToTarget:` (or `-stopDispatchingForProtocol:`),
never relying on `-dealloc`.

**Why.** Objective-C deallocation timing is non-deterministic, so a target that
is only unregistered in `-dealloc` can remain registered after the feature is
gone; the dispatcher then forwards commands to a stopped coordinator and the
next registration of the same protocol fails the dispatcher's
duplicate-registration `CHECK`. See
`ios/architecture/commands.md#configuring-and-stopping-dispatch` and
`ios/architecture/coordinators.md#deterministic-teardown-in`.

## Examples

Bad:

```cpp
- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(PageInfoCommands)];
}

- (void)stop {
  // Missing stopDispatchingToTarget:.
  _viewController = nil;
}
```

Good:

```cpp
- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(PageInfoCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  _viewController = nil;
}
```
