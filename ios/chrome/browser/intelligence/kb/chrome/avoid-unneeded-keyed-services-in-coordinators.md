---
name: ios/avoid-unneeded-keyed-services-in-coordinators
rubric: design
severity: should_fix
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
