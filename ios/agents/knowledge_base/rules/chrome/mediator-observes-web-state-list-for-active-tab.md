---
name: ios/mediator-observes-web-state-list-for-active-tab
description: Inject WebStateList at initialization into a mediator that tracks the active tab, so it observes tab changes directly instead of the coordinator pushing active WebState updates
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Observing Model Objects
    quote: When a mediator cares about the active `WebState` in a `Browser`'s `WebStateList`, it should receive the `WebStateList*` at initialization and observe it directly (or use an observation forwarder for the active `WebState`), rather than having the active `WebState` repeatedly injected from the outside as tabs change.
---

**Rule.** Pass `WebStateList*` to a mediator at initialization when it tracks
the active `WebState` in a `Browser`'s `WebStateList`. The mediator then
observes tab changes directly or via `ActiveWebStateObservationForwarder`. Do
not have the coordinator repeatedly push the active `WebState*` into the
mediator as tabs change.

**Why.** Observing `WebStateList*` directly keeps active-tab tracking inside the
mediator and leaves coordinators focused on UI lifecycle rather than tab-switch
plumbing. See `ios/architecture/mediators.md#observing-model-objects`.

## Examples

Bad:

```cpp
// feature_coordinator.mm
// Bad: Coordinator extracts the active WebState and pushes updates into the
// mediator from outside as tabs change.
- (void)start {
  _viewController = [[FeatureViewController alloc] init];
  _mediator = [[FeatureMediator alloc] init];
  _mediator.consumer = _viewController;
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  _mediator.activeWebState = activeWebState;
}
```

Good:

```cpp
// feature_coordinator.mm
// Good: Coordinator injects WebStateList* at initialization.
- (void)start {
  _viewController = [[FeatureViewController alloc] init];
  _mediator = [[FeatureMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()];
  _mediator.consumer = _viewController;
}

// feature_mediator.mm
@interface FeatureMediator () <WebStateListObserving>
@end

@implementation FeatureMediator {
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _webStateListObservation;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  if ((self = [super init])) {
    _webStateList = webStateList;
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _webStateListObservation = std::make_unique<
        base::ScopedObservation<WebStateList, WebStateListObserver>>(
        _webStateListObserverBridge.get());
    _webStateListObservation->Observe(_webStateList);
  }
  return self;
}

- (void)disconnect {
  _webStateListObservation.reset();
  _webStateListObserverBridge.reset();
  _webStateList = nullptr;
}

- (void)setConsumer:(id<FeatureConsumer>)consumer {
  _consumer = consumer;
  if (_consumer && _webStateList) {
    [self pushActiveTabTitle:_webStateList->GetActiveWebState()];
  }
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change()) {
    [self pushActiveTabTitle:status.new_active_web_state];
  }
}

#pragma mark - Private

- (void)pushActiveTabTitle:(web::WebState*)webState {
  NSString* title =
      webState ? base::SysUTF16ToNSString(webState->GetTitle()) : @"";
  [_consumer setActiveTabTitle:title];
}

@end
```
