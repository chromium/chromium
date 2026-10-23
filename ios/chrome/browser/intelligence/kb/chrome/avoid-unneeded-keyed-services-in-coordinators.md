---
name: ios/avoid-unneeded-keyed-services-in-coordinators
description: When instantiating mediators or injecting dependencies in coordinators, do not retrieve unneeded services via `*Factory::GetForProfile`; pass `nullptr` instead to avoid eager service initialization
rubric: design
severity: should_fix
citations:
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: A special case of base::SupportsUserData::Data is the KeyedService. They are managed by the ProfileKeyedServiceFactoryIOS infrastructure. This infrastructure allows to declare dependencies between services and ensure that they are created and destroyed in an order compatible with those dependencies.
---

**Rule.** Do not retrieve profile keyed services from service factories (such as
`*Factory::GetForProfile(profile)`) in coordinators if the service is not
required for the specific screen or user flow being coordinated. When
instantiating a mediator or child component whose initializer documents a
service dependency as optional (only used in other flows), pass `nullptr` (or
`nil`) rather than eagerly fetching the keyed service; otherwise provide a
narrower initializer so required dependencies remain `CHECK`-asserted.

**Why.** Keyed service factories instantiate services lazily upon the first
query. Retrieving unneeded services in coordinators triggers unnecessary service
initialization, unwanted memory allocation, potential background tasks or disk
I/O, and introduces redundant build dependencies.

## Examples

Bad:

```cpp
// Unconditionally retrieving a keyed service not needed for this flow:
TemplateURLService* templateURLService =
    ios::TemplateURLServiceFactory::GetForProfile(self.profile);

_mediator = [[FooMediator alloc]
    initWithIdentityManager:identityManager
         templateURLService:templateURLService];
```

Good:

```cpp
// Pass `nullptr` when the optional service dependency is not required for this
// flow:
_mediator = [[FooMediator alloc]
    initWithIdentityManager:identityManager
         templateURLService:nullptr];
```
