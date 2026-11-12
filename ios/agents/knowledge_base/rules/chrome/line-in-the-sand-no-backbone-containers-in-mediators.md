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

**Rule.** Never pass backbone containers (`Browser`, `ProfileIOS`, `SceneState`,
`ProfileState`, `AppState`, `ApplicationContext`, or `CommandDispatcher`) into
mediators, view controllers, or views. Never call `GetApplicationContext()` or
keyed-service factories (`*Factory::GetForProfile(...)`) inside a mediator. Have
the coordinator resolve the specific model services and narrow command handlers
(`HandlerForProtocol(..., FooCommands)` into weak `id<FooCommands>` properties)
and inject them explicitly.

**Why.** Passing a backbone container like `Browser*` hides what a mediator
actually uses behind an opaque header. Injecting narrow model services and
protocol-typed command handlers makes each mediator's interactions explicit in
its interface. It also lets unit tests pass simple fakes or mocks without
container setup. See
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
