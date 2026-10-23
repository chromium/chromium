---
name: ios/rely-on-keyed-service-factory-default-incognito-behavior
description: When defining KeyedService factories in iOS Chrome, inherit from ProfileKeyedServiceFactoryIOS, configure traits in the constructor while omitting redundant defaults (NoServiceInIncognito), and do not expose URL/history services in incognito
rubric: design
severity: should_fix
citations:
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: A special case of base::SupportsUserData::Data is the KeyedService. They are managed by the ProfileKeyedServiceFactoryIOS infrastructure.
  - path: chrome/browser/profiles/profile_keyed_service_factory.md
    heading: ProfileSelections
    quote: '* `kOriginalOnly`: Only return the profile if it is an Original Profile, OTR profiles are filtered out (returning nullptr).'
---

**Rule.** When defining KeyedService factories in ios/chrome/browser, inherit
from ProfileKeyedServiceFactoryIOS (or RefcountedProfileKeyedServiceFactoryIOS)
rather than legacy BrowserStateKeyedServiceFactory. Do not override legacy
methods like GetBrowserStateToUse() or ServiceIsNULLWhileTesting(), and do not
pass a dependency manager to the constructor. Configure off-the-record profile
behavior and testing declaratively via constructor traits (such as
ProfileSelection::kRedirectedInIncognito or
TestingCreation::kNoServiceForTests). Omit redundant default traits
(ProfileSelection::kNoInstanceInIncognito, ServiceCreation::kCreateLazily,
TestingCreation::kCreateService), and do not add redundant IsOffTheRecord()
checks in BuildServiceInstanceFor(). Factories for services that track, rank, or
look up visited URLs or browsing history keep the default profile selection (no
incognito instance); never configure `ProfileSelection::kOwnInstanceInIncognito`
for them.

**Why.** ProfileKeyedServiceFactoryIOS modernizes KeyedService management in
ios/chrome/browser by automating DependencyManager registration, marking legacy
context and testing virtual methods final, and configuring behavior via variadic
constructor traits. Inheriting from legacy BrowserStateKeyedServiceFactory or
overriding obsolete methods blocks profile architecture modernization, while
passing default traits or adding manual IsOffTheRecord() guards introduces
redundant boilerplate.

## Examples

Bad:

```cpp
// Bad: Subclassing legacy factory, passing DependencyManager, and overriding obsolete methods:
class TailModelServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  ...
 private:
  web::BrowserState* GetBrowserStateToUse(web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

TailModelServiceFactory::TailModelServiceFactory()
    : BrowserStateKeyedServiceFactory("TailModelService",
                                      BrowserStateDependencyManager::GetInstance()) {}

// Bad: Passing redundant default traits or guarding incognito manually:
CobrowseServiceFactory::CobrowseServiceFactory()
    : ProfileKeyedServiceFactoryIOS("CobrowseService",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    TestingCreation::kCreateService) {}

std::unique_ptr<KeyedService> ImageServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }
  return std::make_unique<ImageService>();
}
```

Good:

```cpp
// Good: Subclass ProfileKeyedServiceFactoryIOS, specify only non-default traits, and let base handle OTR:
class TailModelServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  ...
 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

TailModelServiceFactory::TailModelServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TailModelService",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    TestingCreation::kNoServiceForTests) {}

// Good: Rely on default traits by passing only the service name:
CobrowseServiceFactory::CobrowseServiceFactory()
    : ProfileKeyedServiceFactoryIOS("CobrowseService") {}

std::unique_ptr<KeyedService> ImageServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ImageService>();
}
```
