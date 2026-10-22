---
name: ios/declare-keyed-service-factory-dependencies-with-depends-on
rubric: safety
severity: should_fix
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
