---
name: ios/mediator-deterministic-cleanup-in-disconnect
description: In iOS mediators (-disconnect) and coordinators (-stop), sever UI delegates, reset subscriptions and observer bridges before dependency pointers, and null held raw_ptrs
rubric: safety
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Teardown and -disconnect
    quote: Inside -disconnect, the mediator unregisters all active observations, cancels pending callbacks, and clears its pointers to C++ services and model objects.
  - path: docs/dangling_ptr_guide.md
    heading: Incorrect destruction order
    quote: In the majority of cases, this happens when dependent objects are destroyed in the wrong order in a class, causing the dependency to be released first, thus creating a dangling pointer in the other.
---

**Rule.** In Chrome iOS mediators and coordinators, perform deterministic
cleanup in explicit teardown methods (`-disconnect` for mediators, `-stop` for
coordinators) rather than relying on `-dealloc`. Name explicit teardown in
mediators `-disconnect` rather than `-stop` (which is reserved for
coordinators), reserve it strictly for mandatory pre-nil cleanup rather than
conditional UI dismissal, and document its deallocation contract in the header.
In `-disconnect`, ensure every active observation is torn down and every unowned
dependency is cleared in strict dependency order:

1. Sever UI delegates, consumers, and dispatchers first to prevent asynchronous
   UI callbacks from dereferencing nulled models.
2. Reset subscriptions (`base::CallbackListSubscription` instances via `= {}` or
   `.reset()`), task trackers (`TryCancelAll()`), owned objects, change
   registrars (`RemoveAll()`), scoped observations (`Reset()`), and all C++
   observer bridges (`.reset()`) next.
3. Explicitly null out every held C++ service pointer (`raw_ptr<T>`) and model
   pointer to `nullptr`. Do not leave any service pointers uncleared or observer
   bridges/subscriptions active when disconnecting.

Additionally, in coordinators (`-stop`), call `[_mediator disconnect]` before
releasing the mediator, then tear down the view controller and clear any
delegate or consumer links the coordinator wired up; model subscriptions and
service pointers live in the mediator, not in the coordinator. In view
controllers, provide setter unregistration or explicit teardown instead of
relying solely on `-dealloc`. A class with an explicit teardown method may
verify in `-dealloc` that teardown ran
(`CHECK(!_observer) << "-disconnect must be called before -dealloc";`); when
retrofitting an existing class, use
`CHECK(!_observer, base::NotFatalUntil::M<n>)`.

**Why.** In Objective-C ARC, mediators, coordinators, and view controllers
frequently outlive `-disconnect` or `-stop` while awaiting autorelease pool
draining, transitions, or dismiss animations. Severing UI delegates first
prevents incoming UI events from dereferencing nulled models. Owned objects,
observer bridges, change registrars, and `base::CallbackListSubscription`
instances may access dependencies or trigger callbacks during unregistration;
resetting them before nullifying underlying service or model pointers prevents
dangling pointer crashes and use-after-free bugs. Furthermore, failing to null
out every held `raw_ptr` service member or leaving active subscriptions causes
use-after-free crashes if the object outlives backend keyed services during
profile destruction or feature teardown. See
`ios/architecture/mediators.md#teardown-and` and
`ios/best_practices.md#deallocation-and-deterministic-cleanup`.

## Examples

Bad:

```cpp
// Anti-pattern 1: Mediator destroying in reverse order or omitting raw_ptr/subscription cleanup
- (void)disconnect {
  _historyService = nullptr;
  _prefChangeRegistrar.RemoveAll();  // BAD: May dereference cleared _historyService!
  _consumer = nil;
  // BAD: Missing `_aimEligibilitySubscription = {};` and `_templateURLService = nullptr;`
}

// Anti-pattern 2: Coordinator releasing the mediator without disconnecting it
// and leaving the view controller's observer registered until -dealloc.
- (void)stop {
  _mediator = nil;  // BAD: -disconnect never runs; observers stay registered.
  _viewController = nil;
}
```

Good:

```cpp
// Solution 1: Mediator disconnect with strict teardown order
@interface AutofillAISaveEntityMediator : NSObject
// Disconnects from the model layer. Must be called before deallocation.
- (void)disconnect;
@end

- (void)disconnect {
  _consumer = nil;
  _aimEligibilitySubscription = {};
  _cancelableTaskTracker.TryCancelAll();
  _scopedObservation.Reset();
  _prefChangeRegistrar.RemoveAll();
  _observerBridge.reset();
  _historyService = nullptr;
  _templateURLService = nullptr;
}

// Solution 2: Coordinator stop and ViewController custom setter
- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController.layoutState = nil;  // Custom setter unregisters the observer.
  _viewController = nil;
}

- (void)setLayoutState:(LayoutState*)layoutState {
  if (_layoutState == layoutState) return;
  [_layoutState removeObserver:self];
  _layoutState = layoutState;
  [_layoutState addObserver:self];
}
```
