---
name: ios/line-in-the-sand-no-backbone-containers-in-mediators
description: Do not pass backbone containers or query GetApplicationContext or factories in mediators; inject leaf model services and protocol command handlers from coordinators
rubric: design
severity: blocking
citations:
  - path: ios/architecture/mediators.md
    heading: The "Line in the Sand" for Dependencies
    quote: Notice that in the example above, the coordinator passes the specific `ReadingListService*` into the mediator rather than passing the `ProfileIOS*` (or `Browser*`) from which the `ReadingListService` was retrieved.
  - path: ios/best_practices.md
    heading: Architecture Pattern Best Practices
    quote: 'Enforce the "line in the sand" for backbone containers: Never pass `Browser`, `ProfileIOS`, `CommandDispatcher`, `SceneState`, `ProfileState`, `AppState`, or `ApplicationContext` into mediators, view controllers, or views, and never call `GetApplicationContext()` inside a mediator.'
---

**Rule.** Mediators must never receive, store, or query backbone container
objects or global singletons, including `Browser*`, `ProfileIOS*`, `AppState*`,
`ProfileState*`, `SceneState*`, `CommandDispatcher*`, or
`GetApplicationContext()`, nor import and query KeyedService factories (such as
`*Factory::GetForProfile(...)` or `GetForBrowserState(...)`). Coordinators must
resolve all dependencies—both application-scoped (e.g., local state
`PrefService*`) and profile-scoped (e.g., `TemplateURLService*`,
`IdentityManager*`)—and inject specific leaf model services directly into
mediator initializers or properties. Additionally, coordinators must extract
narrow protocol-typed command handlers via
`HandlerForProtocol(self.browser->GetCommandDispatcher(), ...)` and inject them
into mediators as weak properties
(`@property(nonatomic, weak) id<FooCommands> fooHandler`); a mediator that needs
to trigger a UI change invokes such an injected handler, and never receives the
`CommandDispatcher` to orchestrate presentation itself.

**Why.** Injecting backbone containers or calling `GetApplicationContext()` and
KeyedService factories inside mediators violates the architectural "line in the
sand", breaks multi-window and multi-profile isolation, introduces hidden
dependencies, and tightly couples mediators to dispatcher or global singleton
infrastructure. Mediators should encapsulate business logic over leaf model
services and request UI changes only through narrow command handlers. Passing
narrow protocol-typed handlers (`id<FooCommands>`) enforces interface
segregation and enables hermetic unit testing with mock services and fakes
without requiring factory registrations or dispatcher setups, and keeping
presentation orchestration in coordinators (with simple confirmation alerts
presented by the view controller) preserves proper sequencing of modals and
dialogs without leaking the dispatcher below the line. See
`ios/architecture/mediators.md#the-line-in-the-sand-for-dependencies` and
`ios/best_practices.md#architecture-pattern-best-practices`.

## Examples

Bad:

```cpp
// Bad 1: Passing backbone container or CommandDispatcher to mediator
- (instancetype)initWithBrowser:(Browser*)browser;
@property(nonatomic, weak) CommandDispatcher* dispatcher;
id<QuickDeleteCommands> handler =
    HandlerForProtocol(self.dispatcher, QuickDeleteCommands);
[handler showQuickDeleteAndCanPerformRadialWipeAnimation:YES];

// Bad 2: Calling GetApplicationContext() or querying factories in mediator
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
PrefService* localState = GetApplicationContext()->GetLocalState();
TemplateURLService* service =
    ios::TemplateURLServiceFactory::GetForProfile(self.profile);
```

Good:

```cpp
// Good 1: Mediator takes only specific services and protocol handlers
- (instancetype)initWithLocalState:(PrefService*)localState
                templateURLService:(TemplateURLService*)service;
@property(nonatomic, weak) id<QuickDeleteCommands> quickDeleteHandler;

// Good 2: Coordinator resolves services and wires command handlers
PrefService* localState = GetApplicationContext()->GetLocalState();
TemplateURLService* service =
    ios::TemplateURLServiceFactory::GetForProfile(self.profile);
mediator = [[MyMediator alloc] initWithLocalState:localState
                               templateURLService:service];
mediator.quickDeleteHandler = HandlerForProtocol(
    self.browser->GetCommandDispatcher(), QuickDeleteCommands);
```
