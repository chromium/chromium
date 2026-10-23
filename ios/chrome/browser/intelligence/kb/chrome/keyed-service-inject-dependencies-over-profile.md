---
name: ios/keyed-service-inject-dependencies-over-profile
description: When designing KeyedServices, WebUI handlers, classes, or methods, inject narrow dependencies or attributes directly rather than passing or retaining broad Browser or ProfileIOS containers
rubric: design
severity: should_fix
citations:
  - path: docs/chrome_browser_design_principles.md
    heading: Modularity
    quote: All features should be logically grouped in the directory structure with well-defined API surfaces. Dependencies should be injected during construction.
  - path: docs/chrome_browser_design_principles.md
    heading: 'Simple Tab Feature: `JsOptimizationsPageActionController`'
    quote: It avoids global state and `Browser*` by taking exactly what it needs in its constructor
  - path: docs/ios/objects.md
    heading: ProfileIOS
    quote: A special case of base::SupportsUserData::Data is the KeyedService. They are managed by the ProfileKeyedServiceFactoryIOS infrastructure. This infrastructure allows to declare dependencies between services and ensure that they are created and destroyed in an order compatible with those dependencies.
  - path: docs/ios/objects.md
    heading: BrowserList
    quote: BrowserList is a container owning Browser instances. It is owned by the ProfileIOS and each ProfileIOS has one associated BrowserList.
---

**Rule.** In objects below the line in the sand—mediators, view controllers,
views, coordinator helpers, and model classes (such as tab helpers, bridges, or
utility classes) that are not themselves Browser- or Profile-scoped
owners—inject specific required dependencies (such as PrefService\*,
FullscreenController\*, or feature pointers), profile attributes (such as
profile name or storage path), or bound callbacks directly into constructors,
methods, or setters rather than accepting or retaining broad container objects
(such as Browser\*, ProfileIOS\*, or base::WeakPtr<ProfileIOS>) and extracting
sub-dependencies internally. In local scopes, bind variables directly to the
required sub-service rather than caching parent context objects. In member
fields or asynchronous workflows, retain only the specific required
sub-dependency (such as BrowserList\* or service pointers via raw_ptr) instead
of holding the broad backbone container. Additionally, implement profile-scoped
browser services as KeyedService instances managed by
ProfileKeyedServiceFactoryIOS rather than standalone classes accepting
ProfileIOS\* across member methods, inject their dependencies during
BuildServiceInstanceFor(), and declare dependent services in factory
constructors using DependsOn(). This rule does not apply to objects above the
line that legitimately take Browser\* or ProfileIOS\*: coordinators, browser
agents, scene agents, profile agents, and a keyed-service factory's
BuildServiceInstanceFor().

**Why.** Passing or storing broad container objects (Browser\*, ProfileIOS\*)
turns them into implicit service locators, grants classes unnecessary access to
monolithic state, and introduces coupling to god-object headers and extraction
boilerplate. Retaining container pointers across asynchronous operations also
risks holding stale or dangling references across profile or window lifecycles.
In contrast, injecting narrow dependencies, attributes, and service pointers
directly clarifies minimal requirements, decouples classes from parent
lifecycles, and simplifies unit testing by allowing lightweight mocks rather
than full profile or browser fixtures. Managing profile-scoped services through
KeyedService and ProfileKeyedServiceFactoryIOS further coordinates
initialization and teardown lifecycles via DependsOn(). Coordinators are the
last objects above the line in the sand: they, browser and scene agents, and
keyed-service factories hold backbone containers precisely to find the
dependencies of the objects they create, so the rule targets the objects they
create rather than the owners themselves.

## Examples

Bad:

```cpp
// In a tab helper, retaining parent context locally when only PrefService is
// needed:
ProfileIOS* profile = ProfileIOS::FromBrowserState(web_state->GetBrowserState());
if (IsLensAvailable(profile->GetPrefs())) { ... }

// A model class retaining the broad container in a member field to query
// factories on demand:
class DOMHandler {
  explicit DOMHandler(ProfileIOS* profile) : profile_(profile) {}
  void SetLogging(bool on) {
    FeatureFactory::GetForProfile(profile_)->SetLogging(on);
  }
  raw_ptr<ProfileIOS> profile_;
};

// A coordinator helper taking the container in a method or constructor
// instead of the sub-dependency:
void Helper::SetControllerFromBrowser(Browser* browser) {
  controller_ = FullscreenController::FromBrowser(browser);
}
```

Good:

```cpp
// Bind directly to the required narrow dependency:
PrefService* prefs =
    ProfileIOS::FromBrowserState(web_state->GetBrowserState())->GetPrefs();
if (IsLensAvailable(prefs)) { ... }

// Inject specific dependency directly at construction:
class DOMHandler {
  explicit DOMHandler(Feature* feature) : feature_(feature) {}
  void SetLogging(bool on) { feature_->SetLogging(on); }
  raw_ptr<Feature> feature_;
};

// Inject the specific required dependency directly:
void Helper::SetController(FullscreenController* controller) {
  controller_ = controller;
}

// Above the line: the coordinator resolves the dependency from its Browser*
// and injects it. Coordinators, agents and BuildServiceInstanceFor() are not
// flagged for holding backbone containers.
- (void)start {
  _helper->SetController(FullscreenController::FromBrowser(self.browser));
}
```
