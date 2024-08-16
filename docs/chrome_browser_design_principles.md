These design principles make it easier to write, debug, and maintain desktop
code in //chrome/browser. Most, but not all code in //chrome/browser is desktop
code. Some code is used on Android.

## Caveats:
* These are recommendations, not requirements.
* These are not intended to be static. If you think a
  principle doesn't make sense, reach out to //chrome/OWNERS.
* These are intended to apply to new code and major refactors. We do not expect
  existing features to be refactored, except as need arises.

## Structure, modularity:
* Features should be modular.
    * For most features, all business logic should live in some combination of
      //chrome/browser/<feature>, //chrome/browser/ui/<feature> or
      //component/<feature>.
    * WebUI resources are the only exception. They will continue to live in
      //chrome/browser/resources/<feature> alongside standalone BUILD.gn files.
    * This directory should have a standalone BUILD.gn and OWNERs file.
    * All files in the directory should belong to targets in the BUILD.gn.
        * Do NOT add to `//chrome/browser/BUILD.gn:browser`,
          `//chrome/test/BUILD.gn` or `//chrome/browser/ui/BUILD.gn:ui`.
    * gn circular dependencies are disallowed. Logical
      circular dependencies are allowed (for legacy reasons) but discouraged.
        * [Lens
          overlay](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/lens/BUILD.gn;drc=8e2c1c747f15a93c55ab2f10ebc8b32801ba129e)
          is an example of a feature with no circular dependencies.
            * The BUILD.gn should use public/sources separation.
                * The main reason for this is to guard against future, unexpected usage
                  of parts of the code that were intended to be private. This makes it
                  difficult to change implementation details in the future.
                * This directory may have a public/ subdirectory to enforce further
                  encapsulation, though this example does not use it.
        * [cookie
          controls](https://chromium-review.googlesource.com/c/chromium/src/+/5771416/5/chrome/browser/ui/cookie_controls/BUILD.gn)
          is an example of a feature with logical circular dependencies.
            * The header files are moved into a "cookie_controls" target with no
              circular dependencies.
            * The cc files are moved into a "impl" target, with circular
              dependencies allowed with `//chrome/browser:browser` and
              `//chrome/browser/ui:ui`. These circular dependencies will
              disappear when all sources are removed from `//chrome/browser:browser` and `//chrome/browser/ui:ui`.
            * The separation between header and cc files is functionally
              equivalent to creating abstract base classes in one target, with
              h/cc files in a separate target. This just skips the boilerplate
              of creating the abstract base classes.
            * Even though there are no build circular dependencies, there are
              still logical circular dependencies from the cc files. This
              discrepancy is because C++ allows headers to forward declare
              dependencies, which do not need to be reflected in gn.
    * This directory may have its own namespace.
    * Corollary: There are several global functions that facilitate dependency
      inversion. It will not be possible to call them from modularized features
      (no dependency cycles), and their usage in non-modularized features is
      considered a red flag:
        * `chrome::FindBrowserWithTab` (and everything in browser_finder.h)
        * `GetBrowserViewForNativeWindow`  (via browser_view.h)
        * `FindBrowserWindowWithWebContents` (via browser_window.h)
    * Rationale: Modularity enforces the creation of API surfaces and explicit
      dependencies. This has several positive externalities:
        * Separation of interface from implementation prevents unnecessarly
          tight coupling between features. This in turn reduces spooky action at
          a distance, where seemingly innocuous changes break a distant,
          supposedly unrelated feature.
        * Explicit listing of circular dependencies exposes the likely fragile
          areas of code.
        * Alongside the later guidance of global functions must be pure,
          modularity adds the requirement that test-code perform dependency
          injection. This eliminates a common bug where test behavior diverges
          from production behavior, and logic is added to production code to
          work around test-only behaviors.

* Features should have a core controller with precise lifetime semantics. The
  core controller for most desktop features should be owned and instantiated by
  one of the following classes:
    * `TabFeatures` (member of `TabModel`)
        * This class should own all tab-centric features. e.g. print preview,
          lens overlay, compose, find-in-page, etc.
            * If the feature requires instantiation of
              `WebContents::SupportsUserData`, it should be done in this class.
        * For desktop chrome, `TabHelpers::AttachTabHelpers` will become a
          remove-only method. Clank/WebView may continue to use section 2 of
          `TabHelpers::AttachTabHelpers` (Clank/WebView only).
        * More complex features that also target mobile platforms or other
          supported embedders (e.g. android webview) will continue to use the
          layered components architecture.
            * We defer to //components/OWNERS for expertise and feedback on the
              architecture of these features, and encourage feature-owners to
              proactively reach out to them.
        * Lazy instantiation of `WebContents::SupportsUserData` is an
          anti-pattern.
    * `BrowserWindowFeatures` (member of `Browser`)
        * example: omnibox, security chip, bookmarks bar, side panel
    * `BrowserContextKeyedServiceFactory` (functionally a member of `Profile`)
        * Override `ServiceIsCreatedWithBrowserContext` to return `true`. This
          guarantees precise lifetime semantics.
        * Lazy instantiation is an anti-pattern.
            * Production code is started via `content::ContentMain()`. Test
              harnesses use a test-suite dependent start-point, e.g.
              `base::LaunchUnitTests`. Tests typically instantiate a subset of
              the lazily-instantiated factories instantiated by production code,
              at a different time, with a different order. This results in
              different, sometimes broken behavior in tests. This is typically
              papered over by modifying the production logic to include
              otherwise unnecessary conditionals, typically early-exits.
              Overriding `ServiceIsCreatedWithBrowserContext` guarantees
              identical behavior between test and production code.
        * Use `TestingProfile::Builder::AddTestingFactory` to stub or fake
          services.
        * Separating the .h and .cc file into different build targets is
          allowed.
            * BrowserContextKeyedServiceFactory combines 3 pieces of
              functionality:
                * A getter to receive a service based on an instance of
                  `Profile`.
                * A mechanism to establishing dependencies.
                * A way to glue together //chrome layer logic with //components
                  layer logic.
                * The latter two pieces of functionality are implemented in the
                  .cc file and typically result in dependencies on other parts
                  of //chrome. The first piece of functionality is implemented
                  in the .h file and does not necessarily introduce a dependency
                  on //chrome, since the returned service can be defined in
                  //components.
    * GlobalFeatures.
        * Features which are scoped to the entire process and span multiple
          Profiles should be members of GlobalFeatures.
        * GlobalFeatures is a member of BrowserProcess and they have similar
          lifetime semantics. The main difference is that historically
          BrowserProcess used the antipattern of lazy instantiation, and the
          setup of TestingBrowserProcess encourages divergence between test
          logic and production logic. On the other hand, GlobalFeatures is
          always instantiated.
            * This is not making any statements about initialization (e.g.
              performing non-trivial setup).
    * The core controller should not be a `NoDestructor` singleton.
* Global functions should not access non-global state.
    * Pure functions do not access global state and are allowed. e.g. `base::UTF8ToWide()`
    * Global functions that wrap global state are allowed, e.g.
      `IsFooFeatureEnabled()` wraps the global variable
      `BASE_FEATURE(kFooFeature,...)`
    * Global functions that access non-global state are disallowed. e.g.
      static methods on `BrowserList`.
* No distinction between `//chrome/browser/BUILD.gn` and
  `//chrome/browser/ui/BUILD.gn`
    * There is plenty of UI code outside of the `ui` subdirectory, and plenty of
      non-UI code inside of the `ui` subdirectory. Currently the two BUILD files
      allow circular includes. We will continue to treat these directories and
      BUILD files as interchangeable.

## UI
* Features should use WebUI and Views toolkit, which are x-platform.
    * Usage of underlying primitives is discouraged (aura, Cocoa, gtk, win32,
      etc.). This is usually a sign that either the feature is misusing the UI
      toolkits, or that the UI toolkits are missing important functionality.
* Features should use "MVC"
    * Clear separation between controller (control flow), view (presentation of
      UI) and model (storage of data).
    * For simple features that do not require data persistence, we only require
      separation of controller from view.
    * TODO: work with UI/toolkit team to come up with appropriate examples.
* Views:
    * For simple configuration changes, prefer to use existing setter methods
      and builder syntax.
    * Feel free to create custom view subclasses to encapsulate logic or
      override methods where doing so helps implement layout as the composition
      of smaller standard layouts, or similar. Don't try to jam the kitchen sink
      into a single giant view.
    * However, avoid subclassing existing concrete view subclasses, e.g. to add
      or tweak existing behavior. This risks violating the Google style guidance
      on multiple inheritance and makes maintenance challenging. In particular
      do not do this with core controls, as the behaviors of common controls
      should not vary across the product.
* Avoid subclassing Widgets.
* Avoid self-owned objects/classes for views or controllers.

## General
* Code unrelated to building a web-browser should not live in //chrome.
    * See [chromeos/code.md](chromeos/code.md) for details on ChromeOS (non-browser) code.
    * We may move some modularized x-platform code into //components. The main
      distinction is that ChromeOS can depend on //components, but not on
      //chrome. This will be evaluated on a case-by-case basis.
* Avoid nested message loops.
* Threaded code should have DCHECKs asserting correct sequence.
    * Provides documentation and correctness checks.
    * See base/sequence_checker.h.
* Most features should be gated by base::Feature, API keys and/or gn build
  configuration, not C preprocessor conditionals e.g. `#if
  BUILDFLAG(FEATURE_FOO)`.
    * We ship a single product (Chrome) to multiple platforms. The purpose of
      preprocessor conditionals is:
        * (1) to allow platform-agnostic code to reference platform-specific
          code.
            * e.g. `#if BUILDFLAG(OS_WIN)`
        * (2) to glue src-internal into public //chromium/src.
            * e.g. `#if BUILDFLAG(GOOGLE_CHROME_BRANDING)`
        * (3) To make behavior changes to non-production binaries
            * e.g. `#if !defined(NDEBUG)`
            * e.g. `#if defined(ADDRESS_SANITIZER)`
    * (1) primarily serves as glue.
    * (2) turns Chromium into Chrome. We want the two to be as similar as
      possible. This preprocessor conditional should be used very sparingly.
      Almost all our tests are run against Chromium, so any logic behind this
      conditional will be mostly untested.
    * (3) is a last resort. The point of DEBUG/ASAN/... builds is to provide
      insight into problems that affect the production binaries we ship. By
      changing the production logic to do something different, we are no longer
      accomplishing this goal.
    * In all cases, large segments of code should not be gated behind
      preprocessor conditionals. Instead, they should be pulled into separate
      files via gn.
    * In the event that a feature does have large swathes of code in separate
      build files/translation units (e.g. extensions), using a custom feature
      flag (e.g. `BUILDFLAG(ENABLE_EXTENSIONS)`) to glue this into the main source
      is allowed. The glue code should be kept to a minimum.
* Avoid run-time channel checking.
* Avoid test only conditionals
    * This was historically common in unit_tests, because it was not possible to
      stub out dependencies due to lack of a clear API surface. By requiring
      modular features with clear API surfaces, it also becomes easy to perform
      dependency injection for tests, thereby removing the need for conditionals
      that can be nullptr in tests.
    * In the event that they are necessary, document and enforce via
      `CHECK_IS_TEST()`.
* Avoid unreachable branches.
    * We should have a semantic understanding of each path of control flow. How
      is this reached? If we don't know, then we should not have a conditional.
* For a given `base::Callback`, execution should either be always synchronous, or
always asynchronous. Mixing the two means callers have to deal with two distinct
control flows, which often leads to incorrect handling of one.
Avoid the following:
```cpp
if (result.cached) {
  RunCallbackSync())
} else {
  GetResultAndRunCallbackAsync()
}
```
* Avoid re-entrancy. Control flow should remain as localized as possible.
Bad (unnecessary delegation, re-entrancy)
```cpp
class CarFactory {
  std::unique_ptr<Car> CreateCar() {
    if (!CanCreateCar()) {
      return nullptr;
    }
    if (FactoryIsBusy() && !delegate->ShouldShowCarIfFactoryIsBusy()) {
      return nullptr;
    }
    return std::make_unique<Car>();
  }

  bool CanCreateCar();
  bool FactoryIsBusy();

  Delegate* delegate_ = nullptr;
};

class CarSalesPerson : public Delegate {
  // Can return nullptr, in which case no car is shown.
  std::unique_ptr<Car> ShowCar() {
    return car_factory_->CreateCar();
  }

  // Delegate overrides:
  // Whether the car should be shown, even if the factory is busy.
  bool ShouldShowCarIfFactoryIsBusy() override;

  CarFactory* car_factory_ = nullptr;
};
```

Good, version 1: Remove delegation. Pass all relevant state to CarFactory so that CreateCar() does not depend on non-local state.
```cpp
class CarFactory {
  std::unique_ptr<Car> CreateCar(bool show_even_if_factory_is_busy) {
    if (!CanCreateCar()) {
      return nullptr;
    }
    if (FactoryIsBusy() && !show_even_if_factory_is_busy) {
      return nullptr;
    }
    return std::make_unique<Car>();
  }
  bool CanCreateCar();
  bool FactoryIsBusy();
};

class CarSalesPerson {
  // Can return nullptr, in which case no car is shown.
  std::unique_ptr<Car> ShowCar() {
    return car_factory_->CreateCar(ShouldShowCarIfFactoryIsBusy());
  }

  // Whether the car should be shown, even if the factory is busy.
  bool ShouldShowCarIfFactoryIsBusy();

  CarFactory* car_factory_ = nullptr;
};
```

Good, version 2: Remove delegation. CreateCar always creates a car (fewer conditionals). State only flows from CarFactory to CarSalesPerson (and never backwards).
```cpp
class CarFactory {
  bool CanCreateCar();
  bool FactoryIsBusy();
  // Never returns nullptr.
  std::unique_ptr<Car> CreateCar();
};

class CarSalesPerson {
  // Can return nullptr, in which case no car is shown
  std::unique_ptr<Car> ShowCar() {
    if (!car_factory_->CanCreateCar()) {
      return nullptr;
    }
    if (car_factory_->FactoryIsBusy() && !ShouldShowCarIfFactoryIsBusy()) {
      return nullptr;
    }
    return car_factory_->CreateCar();
  }

  // Whether the car should be shown, even if the factory is busy.
  bool ShouldShowCarIfFactoryIsBusy();
  CarFactory* car_factory_ = nullptr;
};
```

* Circular dependencies are a symptom of problematic design.

Bad. FeatureShowcase depends on FeatureA. FeatureA depends on FeatureB.
FeatureB depends on FeatureShowcase. The root problem is that the design for
FeatureShowcase uses both a pull and a push model for control flow.
```cpp
// Shows an icon per feature. Needs to know whether each icon is visible.
class FeatureShowcase {
  FeatureShowcase() {
    // Checks whether A should be visible, and if so, shows A.
    if (FeatureA::IsVisible())
      ShowFeatureA();
  }

  // Called to make B visible.
  void ShowFeatureB();

};

class FeatureA {
  // Feature A depends on feature B.
  FeatureA(FeatureB* b);

  static bool IsVisible();
};

class FeatureB {
  FeatureB(FeatureShowcase* showcase) {
    if (IsVisible())
      showcase->ShowFeatureB();
  }
  static bool IsVisible();
};
```

Good, version 1. FeatureShowcase uses a pull model for control flow.
FeatureShowcase depends on FeatureA and FeatureB. FeatureA depends on FeatureB.
There is no circular dependency.
```cpp
// Shows an icon per feature. Needs to know whether each icon is visible.
class FeatureShowcase {
  FeatureShowcase() {
    if (FeatureA::IsVisible())
      ShowFeatureA();
    if (FeatureB::IsVisible())
      ShowFeatureB();
  }
};

class FeatureA {
  // Feature A depends on feature B.
  FeatureA(FeatureB* b);

  static bool IsVisible();
};

class FeatureB {
  FeatureB();
  static bool IsVisible();
};
```

Good, version 2. FeatureShowcase uses a push model for control flow. FeatureA
and FeatureB both depend on FeatureShowcase. There is no circular dependency.
```cpp
// Shows an icon per feature. Needs to know whether each icon is visible.
class FeatureShowcase {
  FeatureShowcase();

  // Called to make A visible.
  void ShowFeatureA();

  // Called to make B visible.
  void ShowFeatureB();
};

class FeatureA {
  // Feature A depends on feature B.
  FeatureA(FeatureB* b, FeatureShowcase* showcase) {
    if (IsVisible())
        showcase->ShowFeatureA();
  }

  static bool IsVisible();
};

class FeatureB {
  FeatureB(FeatureShowcase* showcase) {
    if (IsVisible())
        showcase->ShowFeatureB();
  }

  static bool IsVisible();
};
```
