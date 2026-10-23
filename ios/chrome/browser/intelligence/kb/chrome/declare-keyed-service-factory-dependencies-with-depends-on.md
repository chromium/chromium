---
name: ios/declare-keyed-service-factory-dependencies-with-depends-on
description: When implementing KeyedService factory constructors whose service depends on other KeyedServices, declare each dependency using DependsOn(OtherFactory::GetInstance()) to ensure correct destruction ordering
rubric: safety
severity: should_fix
citations:
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: This infrastructure allows to declare dependencies between services and ensure that they are created and destroyed in an order compatible with those dependencies.
  - path: components/keyed_service/README.md
    quote: KeyedService and KeyedServiceFactory together support building a dependency tree of services that are all keyed off the same object (typically, content::BrowserContext or web::BrowserState), and whose teardown order is managed according to the expressed dependency order.
---

**Rule.** In `KeyedService` factory constructors (subclasses of
`ProfileKeyedServiceFactoryIOS` in `//ios/chrome`, or
`BrowserStateKeyedServiceFactory` in `//ios/web_view`), declare all dependencies
on other keyed services using `DependsOn(OtherServiceFactory::GetInstance())`.
Ensure that any `KeyedService` accessed during service creation, operation, or
shutdown is explicitly declared with `DependsOn` so `DependencyManager`
registers the dependency.

**Why.** `DependencyManager` uses declared dependencies to construct the service
dependency graph, determining the initialization and teardown sequence. Omitting
`DependsOn` allows a dependent service to outlive its dependencies during
profile shutdown, causing use-after-free or dangling pointer crashes when
accessing destroyed services.

## Examples

Bad:

```cpp
IOSCloudBinaryUploadServiceFactory::IOSCloudBinaryUploadServiceFactory()
    : ProfileKeyedServiceFactoryIOS("IOSCloudBinaryUploadService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}
```

Good:

```cpp
IOSCloudBinaryUploadServiceFactory::IOSCloudBinaryUploadServiceFactory()
    : ProfileKeyedServiceFactoryIOS("IOSCloudBinaryUploadService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(policy::BrowserManagementServiceFactory::GetInstance());
}
```
