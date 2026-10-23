---
name: ios/local-command-protocols-stay-in-feature-ui
description: Place cross-feature command protocols in shared/public/commands/ dispatched via CommandDispatcher, but keep feature-local protocols in feature UI packages passed directly via handler properties
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Shared vs. Local Command Protocols
    quote: If no other feature needs to call these methods, define a local command protocol inside the feature's ui/ subdirectory rather than shared/public/commands/. Local command protocols are not registered with CommandDispatcher.
---

**Rule.** Any command protocol invoked across features must be defined in its
own header in `//ios/chrome/browser/shared/public/commands/` and dispatched
through `CommandDispatcher`. Conversely, if no other feature needs to call the
command methods, define the feature-local command protocol in the feature's
`ui/` directory (never in `coordinator/`, because `ui/` must not depend on
`coordinator/`), assign the implementing coordinator directly as the
`id<LocalFeatureCommands>` handler on its own view controllers and mediators,
and do not register the protocol with `CommandDispatcher` or resolve it using
`HandlerForProtocol(browser->GetCommandDispatcher(), ...)`. Local command
protocols still follow every command convention (one-way `void` methods, no
UI-layer objects as arguments, weak `<name>Handler` properties). Command passing
between coordinators always goes through `CommandDispatcher`; never pass a local
handler into a child coordinator, which already has a `Browser*`.

**Why.** Centralizing cross-feature commands in
`//ios/chrome/browser/shared/public/commands/` prevents cross-feature DEPS
violations and documents the application's public UI surface. The Browser's
`CommandDispatcher` is designed strictly for cross-feature UI command
dispatching; using it for intra-feature communication pollutes the shared
command namespace, exposes feature-private interfaces across architectural
boundaries, and adds unnecessary coupling to the Browser model instead of direct
coordinator delegation. Declaring the local protocol in `ui/` preserves the
layering rule that `ui/` never imports `coordinator/`, and keeping the command
conventions lets a local protocol be promoted to a shared dispatched protocol
later without changing callers. Conversely, defining globally dispatched
commands in feature-private directories leaks internal feature dependencies
across architectural layers. See `ios/architecture/commands.md` ("Shared vs.
Local Command Protocols") and `ios/architecture/coordinators.md#local-commands`.

## Examples

Bad:

```cpp
// Variant 1: Defining cross-feature command in a feature-private directory
// ios/chrome/browser/bookmarks/ui/bookmarks_commands.h
@protocol BookmarksCommands <NSObject>
- (void)openBookmarkEditor;
@end

// Variant 2: Registering or querying feature-local commands via CommandDispatcher
[dispatcher startDispatchingToTarget:self
                         forProtocol:@protocol(QuickDeleteOtherDataCommands)];
_viewController.quickDeleteOtherDataHandler = HandlerForProtocol(
    self.browser->GetCommandDispatcher(), QuickDeleteOtherDataCommands);

// Variant 3: Declaring the local protocol in coordinator/ and handing the local
// handler to a child coordinator that already has a Browser
// ios/chrome/browser/quick_delete/coordinator/quick_delete_other_data_commands.h
_otherDataCoordinator.quickDeleteOtherDataHandler = self;
```

Good:

```cpp
// Variant 1: Place cross-feature commands in shared/public/commands/
// ios/chrome/browser/shared/public/commands/bookmarks_commands.h
@protocol BookmarksCommands <NSObject>
- (void)openBookmarkEditor;
@end

// Variant 2: Declare the local protocol in ui/ and assign the coordinator
// directly as the handler of its own view controller
// ios/chrome/browser/quick_delete/ui/quick_delete_other_data_commands.h
@protocol QuickDeleteOtherDataCommands <NSObject>
- (void)hideQuickDeleteOtherDataPage;
@end

// quick_delete_coordinator.mm
_viewController.quickDeleteOtherDataHandler = self;
```
