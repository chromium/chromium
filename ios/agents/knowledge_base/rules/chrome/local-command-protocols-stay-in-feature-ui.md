---
name: ios/local-command-protocols-stay-in-feature-ui
description: Define feature-local command protocols in the feature's ui/ directory (or an existing legacy ui_bundled/ until ui/ is split out, but never coordinator/) and pass the coordinator directly as the handler; define cross-feature ones in shared/public/commands/ behind CommandDispatcher
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Shared vs. Local Command Protocols
    quote: If no other feature needs to call these methods, define a **local command protocol** inside the feature's `ui/` subdirectory rather than `shared/public/commands/`. Local command protocols are not registered with `CommandDispatcher`; instead, the implementing coordinator is passed directly as the `id<LocalFeatureCommands>` handler without `HandlerForProtocol()`.
  - path: ios/architecture/coordinators.md
    heading: Local Commands
    quote: For messaging from a feature's view controllers or mediators to its own coordinator, a delegate is not appropriate; instead, define a **local command protocol** in the feature's `ui/` directory and assign the coordinator directly as the handler (without registering on `CommandDispatcher`). Command passing *between* coordinators must always use `CommandDispatcher`.
---

**Rule.** If no other feature calls a command protocol, define it as a local
command protocol in the feature's `ui/` directory, never in `coordinator/` or
`shared/public/commands/`. Legacy features that still combine `ui/` and
`coordinator/` in an existing `ui_bundled/` directory keep local command
protocols there until `ui/` is split out. Never create a new `ui_bundled/`
directory. Assign the implementing coordinator directly as the
`id<LocalFeatureCommands>` handler on its view controllers and mediators,
without registering on `CommandDispatcher` or calling `HandlerForProtocol()`.
Local command protocols still follow all command and handler conventions
(including `weak` `<name>Handler` properties). Define cross-feature command
protocols in their own header in `//ios/chrome/browser/shared/public/commands/`
and dispatch them through `CommandDispatcher`. Command passing between
coordinators always uses `CommandDispatcher`; never pass a local handler to a
child coordinator.

**Why.** Local command protocols keep feature-internal UI actions private, while
shared command headers define the cross-feature UI API. Declaring local
protocols in `ui/` lets view controllers and mediators use them without
depending on `coordinator/`, and lets a protocol be promoted to a shared one
later without changing callers. Routing commands between coordinators through
`CommandDispatcher` keeps coordinators decoupled from each other. See
`ios/architecture/commands.md#shared-vs-local-command-protocols` and
`ios/architecture/coordinators.md#local-commands`.

## Examples

Bad:

```cpp
// Variant 1: Defining a cross-feature command protocol in a feature directory.
// ios/chrome/browser/foo/ui/foo_commands.h
@protocol FooCommands <NSObject>
- (void)showFooEditor;
@end

// Variant 2: Registering or querying a feature-local command protocol via
// CommandDispatcher.
CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
[dispatcher startDispatchingToTarget:self
                         forProtocol:@protocol(FooDetailsCommands)];
_viewController.fooDetailsHandler =
    HandlerForProtocol(dispatcher, FooDetailsCommands);

// Variant 3: Declaring a local command protocol in coordinator/ and passing a
// local handler to a child coordinator.
// ios/chrome/browser/foo/coordinator/foo_details_commands.h
_detailsCoordinator.fooDetailsHandler = self;
```

Good:

```cpp
// Variant 1: Place cross-feature command protocols in shared/public/commands/.
// ios/chrome/browser/shared/public/commands/foo_commands.h
@protocol FooCommands <NSObject>
- (void)showFooEditor;
@end

// Variant 2: Declare the local command protocol in ui/ and assign the
// coordinator directly as the handler on its own view controller.
// ios/chrome/browser/foo/ui/foo_details_commands.h
@protocol FooDetailsCommands <NSObject>
- (void)hideFooDetailsPage;
@end

// ios/chrome/browser/foo/coordinator/foo_coordinator.mm
_viewController.fooDetailsHandler = self;

// Variant 3: Route coordinator-to-coordinator commands through
// CommandDispatcher using a shared protocol.
// ios/chrome/browser/foo/coordinator/foo_details_coordinator.mm
CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
_viewController.fooHandler = HandlerForProtocol(dispatcher, FooCommands);
```
