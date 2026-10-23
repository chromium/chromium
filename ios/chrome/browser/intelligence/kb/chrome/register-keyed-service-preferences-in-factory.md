---
name: ios/register-keyed-service-preferences-in-factory
description: When registering KeyedService preferences, override RegisterProfilePrefs (or RegisterBrowserStatePrefs in BrowserStateKeyedServiceFactory) matching the base factory, rather than registering directly in browser_prefs
rubric: correctness
severity: should_fix
citations:
  - path: docs/patterns/associated-data.md
    heading: KeyedService & BrowserContextKeyedServiceFactory
    quote: 'A third note: on iOS, use [BrowserStateKeyedServiceFactory] instead, which attaches a KeyedService to a [BrowserState].'
  - path: ios/chrome/browser/shared/model/profile/internal/profile_keyed_service_factory_ios.h
    heading: ProfileKeyedServiceFactoryIOS
    quote: Registers any user preferences on this service. This should be overridden by any service that wants to register profile-specific preferences.
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: A special case of base::SupportsUserData::Data is the KeyedService. They are managed by the ProfileKeyedServiceFactoryIOS infrastructure. This infrastructure allows to declare dependencies between services and ensure that they are created and destroyed in an order compatible with those dependencies.
---

**Rule.** When registering profile-scoped preferences for a `KeyedService`,
override the preference registration hook in the service's `KeyedServiceFactory`
subclass rather than registering directly in global preference registration
files (such as `browser_prefs.mm`). Match the exact virtual method of the base
factory: override `RegisterProfilePrefs()` on `ProfileKeyedServiceFactoryIOS`
(in `//ios/chrome`), and override `RegisterBrowserStatePrefs()` on
`BrowserStateKeyedServiceFactory` (in `//ios/web_view`).

**Why.** `KeyedService` factories encapsulate service configuration and
dependencies, allowing `DependencyManager` to register preferences in
topological dependency order during profile initialization. Registering
`KeyedService` preferences in global `browser_prefs.mm` violates encapsulation
and bypasses dependency ordering. Furthermore, declaring
`RegisterProfilePrefs()` on a `BrowserStateKeyedServiceFactory` fails to match
its virtual method, causing compile errors with `override` or silently failing
to register preferences at runtime.

## Examples

Bad:

```cpp
// In ios/chrome/browser/shared/model/prefs/browser_prefs.mm:
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Bad: Registering KeyedService preferences in global browser_prefs.mm.
  registry->RegisterBooleanPref(prefs::kFooServiceEnabled, true);
}

// In ios/web_view/internal/foo/web_view_foo_factory.h:
class WebViewFooFactory : public BrowserStateKeyedServiceFactory {
  // Bad: RegisterProfilePrefs does not match BrowserStateKeyedServiceFactory.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};
```

Good:

```cpp
// In ios/chrome/browser/foo/model/foo_service_factory.h:
class FooServiceFactory : public ProfileKeyedServiceFactoryIOS {
  // Good: Override RegisterProfilePrefs in ProfileKeyedServiceFactoryIOS.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

// In ios/web_view/internal/foo/web_view_foo_factory.h:
class WebViewFooFactory : public BrowserStateKeyedServiceFactory {
  // Good: Override RegisterBrowserStatePrefs in BrowserStateKeyedServiceFactory.
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};
```
