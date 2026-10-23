---
name: ios/place-model-components-in-model-subdirectory
description: When defining model components, business logic wrappers, data models, or service factories, place them in <feature>/model/ rather than coordinator/, ui/, feature root, public/, or ios/public/provider/
rubric: design
severity: should_fix
citations:
  - path: ios/chrome/browser/shared/README.md
    heading: Shared folder
    quote: 'Please add code to one of subfolder: * `coordinator/` if the code is *only* shared by coordinators/mediators * `model/` if the code is *only* shared by model objects * `ui/` if the code is *only* shared by ui elements * `public/` if the code is shared by all Note that coordinators/mediators have access to all sub-folders'
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: A special case of base::SupportsUserData::Data is the KeyedService. They are managed by the ProfileKeyedServiceFactoryIOS infrastructure.
  - path: ios/public/provider/README.md
    quote: This directory exists to allow Chromium code to call iOS code that is not yet upstreamed. The main entry point from Chromium into this directory is via global Provider objects.
---

**Rule.** In iOS Chrome architecture, place model-layer components—keyed
services and their `ProfileKeyedServiceFactoryIOS` factories (along with test
fakes), tab helpers (`web::WebStateUserData`), browser agents
(`BrowserUserData`), tab helper and service observers (`base::CheckedObserver`),
C++-to-Objective-C observer bridges, business logic wrappers and domain model
producers, and domain data structs—in the feature's `model/` directory under
`source_set("model")`. Do not place them in presentation layers (`coordinator/`
or `ui/`, including ad-hoc targets in `coordinator/BUILD.gn`), directly at the
feature root, in the feature's `public/` directory, or in
`ios/public/provider/`. Conversely, UI-ready item and configuration objects
consumed by views (consumer, data source and mutator protocols, composite item
classes, and `*Item`/`*Config` objects populated by a mediator) belong in `ui/`,
not in `model/`. Coordinators, mediators, and external consumers depend on the
feature's `:model` build target instead of defining or consuming domain models
from presentation or provider layers, and `ui/` never imports `model/`.

**Why.** Chrome on iOS organizes features into standardized subdirectories by
layer (`model/`, `coordinator/`, and `ui/`, plus `public/` for constants and
flags). The model layer owns domain state, persistence, and background
observation (keyed services, browser agents, tab helpers, observer bridges),
coordinators and mediators handle view presentation, UI transitions, and scene
orchestration, and `ui/` holds views together with the UI-ready data and
protocols they display. Placing business logic wrappers, service factories, or
domain models in `coordinator/` or `ui/` violates layer separation, pollutes
coordinator responsibilities, impedes testing domain logic independently of UI
flows, and forces UI or external callers to take illegal cross-layer
dependencies on coordinator targets; placing UI item objects in `model/` forces
`ui/` to import `model/`, which it must never do. Similarly, placing model
components directly at the feature root or in `public/` breaks namespace
partitioning and `DEPS` boundaries, as higher layers already depend directly on
`model/`. Finally, `ios/public/provider/` serves strictly as an abstraction
layer for bridging downstream provider implementations; defining browser
`KeyedService`s or model implementations in `public/provider/` violates provider
abstraction boundaries.

## Examples

Bad:

```cpp
// Bad (Variant 1): Business logic wrapper placed in coordinator/.
// ios/chrome/browser/settings/clear_browsing_data/coordinator/browsing_data_counter_wrapper_producer.h
@interface BrowsingDataCounterWrapperProducer : NSObject
- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName;
@end

// Bad (Variant 2): Factory at the feature root, observer in public/, service in
// ios/public/provider/.
// ios/chrome/browser/enterprise/data_protection/data_protection_url_lookup_service_factory.h
class DataProtectionUrlLookupServiceFactory
    : public ProfileKeyedServiceFactoryIOS { /* ... */ };

// ios/chrome/browser/enterprise/data_protection/public/data_protection_tab_helper_observer.h
class DataProtectionTabHelperObserver : public base::CheckedObserver { /* ... */ };

// ios/public/provider/chrome/browser/backend_promo/backend_promo_service.h
class BackendPromoService : public KeyedService { /* ... */ };

// Bad (Variant 3): UI item object consumed by the view placed in model/.
// ios/chrome/browser/foo/model/foo_item.h
@interface FooItem : NSObject
@property(nonatomic, copy) NSString* title;
@end
```

Good:

```cpp
// Good (Variant 1): Model producer placed in <feature>/model/.
// ios/chrome/browser/settings/clear_browsing_data/model/browsing_data_counter_wrapper_producer.h
@interface BrowsingDataCounterWrapperProducer : NSObject
- (std::unique_ptr<BrowsingDataCounterWrapper>)
    createCounterWrapperWithPrefName:(std::string_view)prefName;
@end

// Good (Variant 2): Factory, observer and service placed in <feature>/model/.
// ios/chrome/browser/enterprise/data_protection/model/data_protection_url_lookup_service_factory.h
class DataProtectionUrlLookupServiceFactory
    : public ProfileKeyedServiceFactoryIOS { /* ... */ };

// ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper_observer.h
class DataProtectionTabHelperObserver : public base::CheckedObserver { /* ... */ };

// ios/chrome/browser/backend_promo/model/backend_promo_service.h
class BackendPromoService : public KeyedService { /* ... */ };

// Good (Variant 3): UI item object placed in <feature>/ui/, imported by both
// ui/ and coordinator/.
// ios/chrome/browser/foo/ui/foo_item.h
@interface FooItem : NSObject
@property(nonatomic, copy) NSString* title;
@end
```
