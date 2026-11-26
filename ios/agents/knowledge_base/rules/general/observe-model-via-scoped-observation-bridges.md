---
name: ios/observe-model-via-scoped-observation-bridges
description: In mediators, observe C++ model objects through model-provided observer bridges rather than implementing C++-to-Objective-C bridging logic in the mediator
rubric: readability
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Observing Model Objects
    quote: Because the mediator is an Objective-C object and the model observer interface is usually a C++ pure virtual class, mediators commonly use a C++ **observer bridge** (frequently managed with `base::ScopedObservation`) that forwards C++ observer callbacks to Objective-C methods on the mediator. Implementing an observer bridge is part of the model layer's responsibility; mediators should use model-provided bridges and must not implement C++-to-Objective-C bridging logic themselves.
---

**Rule.** When a mediator observes a C++ model object, use a model-provided C++
observer bridge (often managed with `base::ScopedObservation`) that forwards C++
observer callbacks to Objective-C methods on the mediator. Mediators must not
implement C++-to-Objective-C observer bridging logic themselves.

**Why.** A mediator is an Objective-C object, whereas a model observer interface
is usually a C++ pure virtual class. Implementing the C++ observer bridge that
forwards callbacks to Objective-C methods is part of the model layer's
responsibility, not the mediator's. See
`ios/architecture/mediators.md#observing-model-objects`.

## Examples

Bad:

```cpp
// foo_settings_mediator.mm
namespace {
// Bad: Mediator implements its own C++-to-Objective-C observer bridge.
class FooModelObserverBridge : public FooModel::Observer {
 public:
  FooModelObserverBridge(FooSettingsMediator* mediator, FooModel* model)
      : mediator_(mediator) {
    observation_.Observe(model);
  }
  void OnFooChanged(FooModel* model) override {
    [mediator_ fooModelDidChange:model];
  }

 private:
  __weak FooSettingsMediator* mediator_;
  base::ScopedObservation<FooModel, FooModel::Observer> observation_{this};
};
}  // namespace
```

Good:

```cpp
// foo_settings_mediator.mm
// Good: Mediator uses a model-provided observer bridge.
@interface FooSettingsMediator () <FooModelObserving>
@end

@implementation FooSettingsMediator {
  raw_ptr<FooModel> _fooModel;
  std::unique_ptr<FooModelObserverBridge> _fooModelObserverBridge;
}

- (instancetype)initWithFooModel:(FooModel*)fooModel {
  if ((self = [super init])) {
    _fooModel = fooModel;
    if (_fooModel) {
      _fooModelObserverBridge =
          std::make_unique<FooModelObserverBridge>(self, _fooModel);
    }
  }
  return self;
}

- (void)disconnect {
  _fooModelObserverBridge.reset();
  _fooModel = nullptr;
}

#pragma mark - FooModelObserving

- (void)fooModelDidChange:(FooModel*)fooModel {
  [self.consumer setFooEnabled:fooModel->IsEnabled()];
}

@end
```
