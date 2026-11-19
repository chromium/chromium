---
name: ios/command-handler-property-naming-convention
description: Name weak id<*Commands> properties, ivars, and mocks <feature>Handler or mock_<feature>_handler (never generic handler or *CommandsHandler), and never name protocols or block parameters handler or *Handler
rubric: readability
severity: should_fix
citations:
  - path: ios/architecture/commands.md
    heading: Naming Command Handlers
    quote: Name handler properties and instance variables `<name>Handler`, where `<name>` is the command protocol name *without* the trailing `Commands` suffix
  - path: ios/best_practices.md
    heading: Architecture Pattern Best Practices
    quote: Name handler properties and ivars `<name>Handler` (dropping the `Commands` suffix, e.g., `id<PageInfoCommands> pageInfoHandler`), declare them `weak`, never name them `dispatcher` or bare `handler`, and never combine multiple protocols into one property (`id<FooCommands, BarCommands>`).
  - path: ios/best_practices.md
    heading: Blocks
    quote: 'Never name a block parameter `handler` (or `*Handler`): In Chrome on iOS, "handler" is reserved for [UI command handlers](architecture/commands.md#naming-command-handlers).'
---

**Rule.** When working with command protocols (`id<*Commands>`) and command
dispatching in iOS Chrome:

1. **Handler and Mock Naming**: Name properties, ivars, C++ members, accessors,
   and mocks for command protocols `<feature>Handler`, `_<feature>Handler`,
   `<feature>_handler_`, or `mock_<feature>_handler`, dropping the `Commands` or
   `Command` suffix. Examples include `autofillHandler`, `pageInfoHandler`,
   `_tabGroupsHandler`, `Get<Feature>Handler()`, and
   `mock_settings_handler = OCMProtocolMock(@protocol(SettingsCommands))`.
2. **No Redundant or Generic Names**: Do not include redundant words 'command'
   or 'commands' (avoid `<feature>CommandHandler`, `<feature>CommandsHandler`,
   or `mock_<feature>_command_handler`). Do not name properties or ivars
   generically as `handler` or `commandHandler`, even if the object handles a
   single protocol. Local variables or command-handler parameters may be named
   `<feature>Handler` or simply `handler`.
3. **Reserve `dispatcher` for `CommandDispatcher`**: Reserve `dispatcher`
   strictly for local `CommandDispatcher*` variables and parameters. Objects
   must not declare a `CommandDispatcher*` property or ivar, and must never use
   `dispatcher` or `Get...Dispatcher()` for command protocol endpoints
   (`id<*Commands>`).
4. **Reserve `Handler` for Command Protocol Endpoints**: Do not suffix
   Objective-C protocols with `*Handler`; name them by their architectural role
   (`*Commands` for `CommandDispatcher` routing via `HandlerForProtocol`,
   `*Delegate` for direct delegation, or `*Consumer`). Never name a block
   parameter `handler` or `*Handler`; use `callback` (when returning
   information), `completion` (when signaling an asynchronous operation has
   finished), or another specific descriptive name.
5. **Weak Properties**: Always declare command handler properties and ivars
   `weak` (`@property(nonatomic, weak) id<...Commands> ...` or
   `__weak id<...Commands> _...`) to prevent retain cycles.

**Why.** `CommandDispatcher` routes UI commands across the browser, while
callers hold weak `<feature>Handler` protocol endpoints (`id<*Commands>`) to
keep dependencies explicit and avoid retain cycles. Reserving `<feature>Handler`
for those endpoints and `dispatcher` for local `CommandDispatcher*` variables
prevents ambiguity when objects use multiple command protocols, delegates, or
block callbacks. See `ios/architecture/commands.md#naming-command-handlers`,
`ios/best_practices.md#architecture-pattern-best-practices`, and
`ios/best_practices.md#blocks`.

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

// Bad: Suffixing protocols with Handler, or naming block parameters handler /
// *Handler
@protocol OmniboxLensHandler <NSObject>
- (void)dismissWithCompletionHandler:(ProceduralBlock)handler;
```

Good:

```cpp
// Good: Name properties, ivars, and mocks <feature>Handler without 'command'
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;
@property(nonatomic, weak) id<AutofillCommands> autofillHandler;
id mock_settings_handler =
    OCMProtocolMock(@protocol(SettingsCommands));

// Good: Explicit <feature>Handler naming and HandlerForProtocol accessor
@property(nonatomic, weak) id<PageInfoCommands> pageInfoHandler;
__weak id<TabGroupsCommands> _tabGroupsHandler;
id<SearchEngineChoiceCommands> handler =
    GetSearchEngineChoiceHandler(sceneState);
id<SnackbarCommands> snackbarHandler =
    HandlerForProtocol(self.browser->GetCommandDispatcher(), SnackbarCommands);

// Good: Name protocols and block parameters after their role (*Delegate,
// completion / callback)
@protocol OmniboxLensDelegate <NSObject>
- (void)dismissWithCompletion:(ProceduralBlock)completion;
```
