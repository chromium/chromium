---
name: ios/command-handler-property-naming-convention
description: When managing command protocols (`id<*Commands>`), name properties, ivars, test mocks, and accessors `<feature>Handler` or `mock_<feature>_handler`, omitting redundant 'command', generic 'handler', or 'dispatcher'.
rubric: readability
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Naming Command Handlers
    quote: Name handler properties and instance variables `<name>Handler`, where `<name>` is the command protocol name *without* the trailing `Commands` suffix
  - path: ios/best_practices.md
    heading: Architecture Pattern Best Practices
    quote: Name handler properties and ivars `<name>Handler` (dropping the `Commands` suffix, e.g., `id<PageInfoCommands> pageInfoHandler`), declare them `weak`, never name them `dispatcher` or bare `handler`, and never combine multiple protocols into one property (`id<FooCommands, BarCommands>`).
---

**Rule.** When working with command protocols (`id<*Commands>`) and command
dispatching in iOS Chrome:

1. **Handler and Mock Naming**: Name properties, instance variables, C++ member
   variables, helper accessors, and test mocks holding or returning command
   protocol implementations `<feature>Handler`, `_<feature>Handler`,
   `<feature>_handler_`, or `mock_<feature>_handler` (dropping the trailing
   `Commands` or `Command` suffix, e.g., `autofillHandler`, `pageInfoHandler`,
   `_tabGroupsHandler`, `Get<Feature>Handler()`,
   `mock_settings_handler = OCMProtocolMock(@protocol(SettingsCommands))`).
2. **No Redundant or Generic Names**: Do not include redundant words 'command'
   or 'commands' (avoid `<feature>CommandHandler`, `<feature>CommandsHandler`,
   or `mock_<feature>_command_handler`). Do not name properties or ivars
   generically as `handler`, `commandHandler`, or `dispatcher`, even if the
   object handles a single protocol. Local variables or parameters may be named
   `<feature>Handler` or simply `handler`.
3. **Reserve `dispatcher` for `CommandDispatcher`**: Reserve `dispatcher`
   strictly for `CommandDispatcher*` instances; never use `dispatcher` or
   `Get...Dispatcher()` for command protocol endpoints (`id<*Commands>`).
4. **Reserve `Handler` for Command Protocol Endpoints**: Do not suffix
   Objective-C protocols with `*Handler`; name them by their architectural role
   (`*Commands` for `CommandDispatcher` routing via `HandlerForProtocol`,
   `*Delegate` for direct delegation, or `*Consumer`). Likewise, avoid naming
   non-command helper classes with a `-Handler` suffix (e.g., use `Opener`
   instead of `LinkOpeningHandler`).
5. **Weak Properties**: Always declare command handler properties and ivars
   `weak` (`@property(nonatomic, weak) id<...Commands> ...` or
   `__weak id<...Commands> _...`) to prevent retain cycles.

**Why.** In iOS Chrome architecture, `CommandDispatcher` is the central routing
engine that multiplexes UI commands, while callers interact with specific
features via narrow command protocol interfaces (`id<*Commands>`) registered
with the dispatcher and accessed via `HandlerForProtocol`. Suffixing protocols
with `*Handler` confuses the protocol interface with the conforming endpoint,
while naming non-command helper classes or delegates with `-Handler` confuses
internal helpers with Chromium's command routing architecture. Repeating
'command' or 'commands' in handler, ivar, or mock names (e.g.,
`<feature>CommandHandler` or `mock_<feature>_command_handler`) adds redundant
boilerplate and departs from established subsystem conventions. Using generic
names like `handler` on properties or ivars causes ambiguity and naming
collisions when coordinators, view controllers, or mediators consume multiple
command protocols. Similarly, using `dispatcher` for command protocol pointers
obscures the distinction between the underlying multi-protocol dispatch
container (`CommandDispatcher`) and the specific protocol handler. Finally,
command handler properties must be held weakly to avoid circular reference leaks
between coordinators and UI components. See
`ios/architecture/commands.md#naming-command-handlers` and
`ios/best_practices.md#architecture-pattern-best-practices`.

## Examples

Bad:

```cpp
// Bad: Redundant 'command' in property, ivar, and test mock names
@property(nonatomic, weak) id<SettingsCommands> settingsCommandHandler;
@property(nonatomic, weak) id<AutofillCommands> autofillCommandsHandler;
id mock_settings_command_handler =
    OCMProtocolMock(@protocol(SettingsCommands));

// Bad: Generic, dispatcher, or strong references for command handlers
@property(nonatomic, strong) id<PageInfoCommands> handler;
@property(nonatomic, weak) id<SettingsCommands> settingsDispatcher;
auto dispatcher = GetSearchEngineChoiceCommandsDispatcher(sceneState);

// Bad: Suffixing protocols or non-command helper classes with Handler
@protocol OmniboxLensHandler <NSObject>
@interface GeminiLinkOpeningHandler : NSObject <GeminiLinkOpeningDelegate>
```

Good:

```cpp
// Good: Name properties, ivars, and test mocks <feature>Handler without 'command'
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;
@property(nonatomic, weak) id<AutofillCommands> autofillHandler;
id mock_settings_handler =
    OCMProtocolMock(@protocol(SettingsCommands));

// Good: Explicit <feature>Handler naming and HandlerForProtocol accessor
@property(nonatomic, weak) id<PageInfoCommands> pageInfoHandler;
__weak id<TabGroupsCommands> _tabGroupsHandler;
id<SearchEngineChoiceCommands> handler = GetSearchEngineChoiceHandler(sceneState);
id<SnackbarCommands> snackbarHandler =
    HandlerForProtocol(self.browser->GetCommandDispatcher(), SnackbarCommands);

// Good: Name protocols and helpers after architectural role (*Delegate, *Opener)
@protocol OmniboxLensDelegate <NSObject>
@interface GeminiLinkOpener : NSObject <GeminiLinkOpeningDelegate>
```
