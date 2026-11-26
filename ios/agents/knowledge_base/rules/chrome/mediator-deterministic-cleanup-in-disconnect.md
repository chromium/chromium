---
name: ios/mediator-deterministic-cleanup-in-disconnect
description: Perform deterministic mediator cleanup in -disconnect rather than -dealloc, stopping observations, callbacks, and timers before clearing unowned C++ pointers
rubric: safety
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Teardown and `-disconnect`
    quote: Inside `-disconnect`, the mediator unregisters all active observations, cancels pending callbacks, and clears its pointers to C++ services and model objects.
  - path: ios/best_practices.md
    heading: Deallocation and Deterministic Cleanup
    quote: Mediators reset C++ observer bridges and clear unowned C++ pointers in `-disconnect`
  - path: docs/dangling_ptr_guide.md
    heading: Incorrect destruction order
    quote: In the majority of cases, this happens when dependent objects are destroyed in the wrong order in a class, causing the dependency to be released first, thus creating a dangling pointer in the other.
---

**Rule.** Perform deterministic cleanup in `-disconnect`, not `-dealloc`,
whenever a mediator holds active observations, pending callbacks, timers, or
unowned C++ pointers. Inside `-disconnect`, unregister active observations,
cancel pending callbacks, and stop timers before destroying observer bridges or
owned C++ objects. Finally, clear all unowned C++ service and model pointers
(`raw_ptr<T>`) to `nullptr`. Simple mediators holding none of these may omit
`-disconnect`.

**Why.** Objective-C `-dealloc` timing is indeterminate because UIKit, blocks,
or autorelease pools may retain a mediator after its feature closes.
Deterministic cleanup in `-disconnect` ends observations, callbacks, and timers
before observer bridges, owned objects, and `raw_ptr<T>` dependencies are
released, preventing dangling pointers. See
`ios/architecture/mediators.md#teardown-and-disconnect` and
`ios/best_practices.md#deallocation-and-deterministic-cleanup`.

## Examples

Bad:

```cpp
@interface FeatureMediator : NSObject
- (void)disconnect;
@end

@implementation FeatureMediator {
  raw_ptr<WebStateList> _webStateList;
  raw_ptr<PrefService> _prefService;
  base::CallbackListSubscription _eligibilitySubscription;
  base::CancelableTaskTracker _taskTracker;
  PrefChangeRegistrar _prefChangeRegistrar;
  std::unique_ptr<WebStateListObserverBridge> _observerBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _scopedObservation;
}

- (void)disconnect {
  _webStateList = nullptr;
  // BAD: Destroying _observerBridge before _scopedObservation dangles the
  // bridge pointer still held by _scopedObservation during unregistration.
  _observerBridge.reset();
  _scopedObservation.reset();
  // BAD: Omits _eligibilitySubscription and _taskTracker cleanup, and defers
  // remaining C++ teardown to -dealloc.
}

- (void)dealloc {
  // BAD: Never rely on non-deterministic -dealloc for C++ cleanup.
  _prefChangeRegistrar.RemoveAll();
  _prefService = nullptr;
}

@end
```

Good:

```cpp
@interface FeatureMediator : NSObject
// Disconnects from the model layer.
- (void)disconnect;
@end

@implementation FeatureMediator {
  raw_ptr<WebStateList> _webStateList;
  raw_ptr<PrefService> _prefService;
  base::CallbackListSubscription _eligibilitySubscription;
  base::CancelableTaskTracker _taskTracker;
  PrefChangeRegistrar _prefChangeRegistrar;
  std::unique_ptr<WebStateListObserverBridge> _observerBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _scopedObservation;
}

- (void)disconnect {
  _eligibilitySubscription = {};
  _taskTracker.TryCancelAll();
  _prefChangeRegistrar.RemoveAll();
  _scopedObservation.reset();
  _observerBridge.reset();
  _webStateList = nullptr;
  _prefService = nullptr;
}

@end
```
