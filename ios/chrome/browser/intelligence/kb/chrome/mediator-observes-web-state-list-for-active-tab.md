---
name: ios/mediator-observes-web-state-list-for-active-tab
description: When mediators track active tabs, inject WebStateList directly to observe transitions; when mediators do not observe WebStateList, inject specific sub-dependencies directly
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Observing Model Objects
    quote: When a mediator cares about the active WebState in a Browser's WebStateList, it should receive the WebStateList* at initialization and observe it directly, rather than having the active WebState repeatedly injected.
---

**Rule.** When a mediator needs to observe or track active tab transitions,
inject `WebStateList*` directly at initialization so the mediator can manage tab
observations internally, rather than having coordinators push active `WebState*`
references on tab changes. Conversely, if a mediator does not observe or manage
`WebStateList` and only needs a specific sub-dependency (such as an
`id<FormInputSuggestionsProvider>` or delegate), do not pass broad container
objects like `WebStateList` or `Browser` solely to extract the dependency in
`init`; inject the specific dependency directly via initialization or
properties.

**Why.** In Chrome for iOS architecture, mediators that observe `WebStateList`
encapsulate tab tracking logic and keep coordinators declarative. However,
passing broad container objects to mediators that do not observe them
unnecessarily couples components to `WebStateList` or `Browser`, obscures real
dependencies, and bloats unit tests with complex container fixtures instead of
simple protocol mocks. See
`ios/architecture/mediators.md#observing-model-objects`.

## Examples

Bad:

```cpp
// Bad 1: Coordinator extracts active tab delegates or pushes active tab
webauthn::IOSWebAuthnCredentialsDelegate* delegate =
    factory->GetDelegateForFrame(_requestInfo->frame_id);
_mediator = [[PasskeySuggestionBottomSheetMediator alloc]
    initWithDelegate:delegate];

// Bad 2: Mediator takes WebStateList merely to extract a nested provider
- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  if ((self = [super init])) {
    web::WebState* webState = webStateList->GetActiveWebState();
    _provider = FormSuggestionTabHelper::FromWebState(webState)
                    ->GetAccessoryViewProvider();
  }
  return self;
}
```

Good:

```cpp
// Good 1: Mediator receives WebStateList to observe tab transitions internally
_mediator = [[PasskeySuggestionBottomSheetMediator alloc]
    initWithWebStateList:webStateList];
// Inside mediator: observes WebStateList transitions via WebStateListObserverBridge

// Good 2: Inject narrow provider/delegate directly when mediator does not observe list
- (instancetype)initWithProvider:(id<FormInputSuggestionsProvider>)provider {
  if ((self = [super init])) {
    _provider = provider;
  }
  return self;
}
```
