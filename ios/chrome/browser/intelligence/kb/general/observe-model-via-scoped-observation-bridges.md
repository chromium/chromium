---
name: ios/observe-model-via-scoped-observation-bridges
description: When bridging C++ models to Objective-C++, reuse standard shared `<Type>ObserverBridge` classes with `base::ScopedObservation` and weak delegates instead of declaring private bridges or callbacks
rubric: readability
severity: should_fix
citations:
  - path: components/signin/public/identity_manager/objc/README.md
    heading: Interface
    quote: C++ bridge class implementing `signin::IdentityManager::Observer`. Observes a `signin::IdentityManager` instance via `base::ScopedObservation` and forwards dispatched events to an `id<IdentityManagerObserving>` target.
  - path: ios/architecture/mediators.md
    heading: Observing Model Objects
    quote: Because the mediator is an Objective-C object and the model observer interface is usually a C++ pure virtual class, mediators commonly use a C++ observer bridge (frequently managed with base::ScopedObservation) that forwards callbacks.
---

**Rule.** When bridging C++ observer events to Objective-C consumers, follow the
standard `<Type>ObserverBridge` pattern: implement the C++ observer interface in
a dedicated bridge class that lives in the model layer, declare an Objective-C
`<Type>Observing` protocol whose methods use Objective-C `did`/`will` naming
(never the C++ `On*`/`on*` style), hold a `__weak` reference to the observing
object, and manage observation lifecycles via `base::ScopedObservation` instead
of ad-hoc block callbacks or unmanaged observer registrations. Reuse existing
shared `<Type>ObserverBridge` classes rather than declaring duplicate local
bridge classes in anonymous namespaces or implementation files; mediators must
not implement C++-to-Objective-C bridging logic themselves. If an existing
shared bridge lacks an observer method needed by a new consumer, extend the
shared bridge class and its `<Type>Observing` protocol rather than defining a
private bridge.

**Why.** In Chromium iOS, bridging C++ observer notifications through canonical
`<Type>ObserverBridge` classes and `<Type>Observing` protocols standardizes
event dispatch across mediators and consumers while eliminating duplicate
boilerplate. Defining private observer bridges in anonymous namespaces or using
ad-hoc block callbacks leads to redundant implementations, inconsistent event
handling, and maintenance drift. Holding `__weak` references to the observing
object avoids retain cycles with owning consumers and prevents messaging
deallocated objects, while `base::ScopedObservation` guarantees automatic
detachment upon bridge destruction, preventing dangling observer pointers and
use-after-free crashes. See
`ios/architecture/mediators.md#observing-model-objects` and
`ios/best_practices.md#protocol-naming-and-design-conventions`.

## Examples

Bad:

```cpp
// Bad: Using ad-hoc block callbacks to bridge C++ observer events.
class FooModelObserver : public FooModel::Observer {
 public:
  typedef void (^ModelReadyCallback)(FooModel*);
  FooModelObserver(FooModel* model, ModelReadyCallback cb);
 private:
  ModelReadyCallback callback_;
};

// Bad: Duplicating an observer bridge locally in an anonymous namespace.
namespace {
class FooModelObserverBridge : public FooModel::Observer {
 public:
  FooModelObserverBridge(FooSettingsMediator* owner, FooModel* model)
      : owner_(owner) {
    observation_.Observe(model);
  }
  void OnItemsChanged(const std::vector<FooModelChange>& changes) override {
    [owner_ onItemsChanged];  // Bad: on* selector on the Objective-C side.
  }

 private:
  __weak FooSettingsMediator* owner_;
  base::ScopedObservation<FooModel, FooModel::Observer> observation_{this};
};
}  // namespace
```

Good:

```cpp
// Good: Shared ObserverBridge in model/ with a weak <Type>Observing reference and
// ScopedObservation; Objective-C selectors use did/will naming.
// ios/chrome/browser/foo/model/foo_model_observer_bridge.h
@protocol FooModelObserving <NSObject>
- (void)fooModelDidBecomeReady:(FooModel*)fooModel;
- (void)fooModelDidChange:(FooModel*)fooModel;
@end

class FooModelObserverBridge : public FooModel::Observer {
 public:
  FooModelObserverBridge(id<FooModelObserving> observer,
                             FooModel* model);
 private:
  __weak id<FooModelObserving> observer_;
  base::ScopedObservation<FooModel,
                          FooModel::Observer>
      scoped_observation_{this};
};

// Good: Reusing the shared model bridge and adopting its protocol.
#import "ios/chrome/browser/foo/model/foo_model_observer_bridge.h"

@interface FooSettingsMediator () <FooModelObserving>
@end

@implementation FooSettingsMediator {
  std::unique_ptr<FooModelObserverBridge> _fooModelObserverBridge;
}

- (void)startObservingModel:(FooModel*)model {
  _fooModelObserverBridge =
      std::make_unique<FooModelObserverBridge>(self, model);
}

// FooModelObserving
- (void)fooModelDidChange:(FooModel*)fooModel {
  [self pushItemsToConsumer];
}
@end
```
