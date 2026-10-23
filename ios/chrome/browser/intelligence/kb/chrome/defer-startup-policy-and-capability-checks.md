---
name: ios/defer-startup-policy-and-capability-checks
description: When initializing KeyedServices or handling startup in profile-scoped components, do not eagerly prefetch policy or capability checks; defer them until UI presentation
rubric: design
severity: should_fix
---

**Rule.** Do not eagerly initiate or prefetch expensive or asynchronous
enterprise policy, remote capability, or network checks in `KeyedService`
constructors or during profile startup. Instead, evaluate or fetch capabilities
lazily on demand (such as inside `Check...IfNeeded()` or when a mediator or
model client queries the service).

**Why.** Eagerly executing asynchronous remote or policy checks during
`KeyedService` construction increases browser startup latency, consumes
background task threads, and causes regressions in startup guardrail metrics.
Deferring verification until a mediator or client actually queries the service
ensures checks run only when needed while keeping profile initialization fast.

## Examples

Bad:

```cpp
// Bad: Eagerly prefetching remote or enterprise policy checks at service construction.
FooServiceImpl::FooServiceImpl(ProfileIOS* profile,
                               AuthenticationService* auth_service)
    : profile_(profile), auth_service_(auth_service) {
  CheckEnterpriseEligibility();
}
```

Good:

```cpp
// Good: Defer capability or policy checks until a mediator or client queries the service.
FooServiceImpl::FooServiceImpl(ProfileIOS* profile,
                               AuthenticationService* auth_service)
    : profile_(profile), auth_service_(auth_service) {}

- (void)setConsumer:(id<FooConsumer>)consumer {
  _consumer = consumer;
  _fooService->CheckEnterpriseEligibilityIfNeeded();
}
```
