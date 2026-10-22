# Chrome on iOS Best Practices

> **Scope:** This document provides tactical coding guidelines for day-to-day
> development in `//ios/`. Architectural theory, design principles, and pattern
> specifications live in [Chrome on iOS Architecture](architecture/README.md).
> Many rules here are also enforced automatically by
> [`//ios/PRESUBMIT.py`](PRESUBMIT.py).

[TOC]

______________________________________________________________________

## Working in the Codebase

### Style and Reference Guides

Code in `//ios/` follows these upstream style and engineering guides except
where noted in this document:

- **Objective-C:**
  [Google Objective-C Style Guide](https://google.github.io/styleguide/objcguide.html)
  and
  [Chromium Objective-C Style Guide](/styleguide/objective-c/objective-c.md).
- **C++:**
  [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html),
  [Chromium C++ Style Guide](/styleguide/c++/c++.md),
  [C++ Dos and Don'ts](/styleguide/c++/c++-dos-and-donts.md),
  [Allowed/Banned C++ Features](/styleguide/c++/c++-features.md),
  [`CHECK()`, `DCHECK()`, and `NOTREACHED()`](/styleguide/c++/checks.md), and
  [General Naming Rules](https://google.github.io/styleguide/cppguide.html#General_Naming_Rules).
- **Swift:** [Google Swift Style Guide](https://google.github.io/swift/).
- **Code Review & CLs:** Google Engineering Practices guides for
  [CL authors](https://google.github.io/eng-practices/review/developer/),
  [reviewers](https://google.github.io/eng-practices/review/reviewer/),
  [small CLs](https://google.github.io/eng-practices/review/developer/small-cls.html),
  and
  [CL descriptions](https://google.github.io/eng-practices/review/developer/cl-descriptions.html).
- **Testing:**
  [Chromium Testing Documentation](/docs/testing/testing_in_chromium.md).

### Modifying Existing and Legacy Code

Chrome on iOS is a large, long-lived codebase; older files often predate current
patterns and conventions. When modifying existing code:

1. **Don't make it worse.** Never extend existing antipatterns or architectural
   violations. New code added to a legacy file must follow current rules (for
   example, proper block weak-self handling or narrow dependency injection), and
   any existing block or method you modify should be updated to match current
   best practices.
2. **Clean up as you go.** Fix small issues in code you touch—typos in comments
   or identifiers, method grouping, or dead methods and classes. When deleting
   dead code, also delete any newly unreferenced helpers, `#import` directives,
   and `BUILD.gn` dependencies.
3. **Keep individual files internally consistent.** Mechanical cleanups (such as
   updating variable naming or reordering header declarations) should be applied
   to the entire class or header so a single file does not mix conflicting
   styles. Put larger mechanical cleanups in a precursor or follow-up CL to keep
   functional diffs easy to review.
4. **No deadline exceptions.** Milestone branch dates and feature freezes do not
   relax code quality or architectural requirements. The only exception is an
   urgent release-branch fix for a severe production bug where reverting is
   riskier than a targeted workaround—and such a fix must include a `TODO` and a
   follow-up cleanup bug.

### Ordering Stacked CLs

- **Never submit dead code.** When splitting a feature across a chain of CLs,
  start from the integration point in existing code and build inward. For
  example, when adding a new UI feature, introduce the coordinator and wire it
  into its parent coordinator first, then add the view controller and mediator
  in follow-up CLs.

### Comments, `TODO`s, and Bug Hygiene

- **Quote symbols with backticks:** In comments, enclose variable, method, and
  class names in backticks (`` `foo` ``), never pipes (`|foo|`). (Enforced by
  [`PRESUBMIT.py`](PRESUBMIT.py).)
- **`TODO` format:** Every `TODO` comment must reference an open issue using
  `TODO(crbug.com/123456): Description`. Do not reference umbrella tracking bugs
  in `TODO`s; file and reference a specific blocking bug instead. (Enforced by
  [`PRESUBMIT.py`](PRESUBMIT.py).)
- **Clean up `TODO`s when closing bugs:** Any CL that resolves a bug must delete
  the corresponding `TODO`s. Never close a bug (as Fixed, Obsolete, or Won't
  Fix) while `TODO`s referencing it remain in the codebase.
- **Closing fixed bugs via CL description:** You can use `Fixed: <bug number>`
  (instead of `Bug: <bug number>`) in the CL description to automatically mark a
  bug Fixed when the CL lands, but only do so if the CL completely resolves the
  issue and removes all associated `TODO`s.

______________________________________________________________________

## Baseline Feature Support

Every feature in Chrome on iOS must support three baseline capabilities from the
start of development:

- **iPad, iPhone Duo, and adaptive layout:** All UI must work across all
  supported device form factors—including iPhone, iPhone Duo, and iPad—and
  inside resizable multi-tasking windows. Drive layout changes from trait
  collections (`UITraitCollection` size classes) rather than device model checks
  or hardcoded screen dimensions.
- **Multiple windows (`UIScene`):** Users can open multiple Chrome windows side
  by side. Never assume a single window or a single active tab list. Scope
  objects to a specific `SceneState` or `Browser` (see
  [Coordinator Scope](architecture/coordinators.md#coordinator-scope)).
- **Multiple user identities (`ProfileIOS`):** Multiple profiles can exist
  concurrently; there is no global singleton `ProfileIOS`. Coordinators resolve
  the appropriate `Browser` and `ProfileIOS` and inject specific services into
  mediators (see
  [The "Line in the Sand" for Dependencies](architecture/mediators.md#the-line-in-the-sand-for-dependencies)).

______________________________________________________________________

## Language Choice

- **Objective-C++ (`.mm`):** In `//ios/`, "Objective-C" always means
  Objective-C++ (`.mm`), which allows freely mixing Objective-C and C++. Plain
  Objective-C (`.m`) is not used outside `//ios/third_party/`. Objective-C++ is
  the default language for UI code (`UIView`, `UIViewController`),
  [coordinators](architecture/coordinators.md),
  [mediators](architecture/mediators.md), `AppState`/`SceneState` agents, and
  inter-layer protocols ([commands](architecture/commands.md),
  [consumers, data sources, and mutators](architecture/mediators.md#consumer-protocols)).
- **C++ (`.cc`, `.h`):** The default language for cross-platform code, the web
  layer (`//ios/web/`), and model-layer components (keyed services, tab helpers,
  browser agents).
- **Swift (`.swift`):** May be used in place of Objective-C for isolated UI
  layer classes (`UIView` and `UIViewController` subclasses) where C++
  interoperability constraints permit. Coordinate with `//ios/` owners before
  introducing Swift to a new target, as Swift usage is monitored for build-time
  and binary-size impact.
- **SwiftUI:**
  - Used where required by system frameworks (such as WidgetKit).
  - In the main app, SwiftUI may be used for simple, self-contained UI surfaces
    without complex cross-feature interactions (see
    [Mediators for Unusual UI Implementations](architecture/mediators.md#mediators-for-unusual-ui-implementations));
    coordinate with `//ios/` owners before starting.
  - **Timebox SwiftUI debugging:** If an intractable SwiftUI framework bug is
    not resolved within 1–2 engineer-days, **discard the SwiftUI implementation
    and rebuild the view in UIKit**.
- **TypeScript (`.ts`):** All scripts injected into web views (`WKWebView`) for
  feature integration must be written in TypeScript, never raw JavaScript.

______________________________________________________________________

## Architecture Pattern Best Practices

Full architectural guidance lives in
[`//ios/architecture/`](architecture/README.md). Follow these tactical rules in
all feature code:

- **Enforce the "line in the sand" for backbone containers:** Never pass
  `Browser`, `ProfileIOS`, `CommandDispatcher`, `SceneState`, `ProfileState`,
  `AppState`, or `ApplicationContext` into mediators, view controllers, or
  views, and never call `GetApplicationContext()` inside a mediator.
  Coordinators extract the specific services, model objects, and command
  handlers their children need and inject them explicitly. See
  [Line in the Sand](architecture/README.md#line-in-the-sand) and
  [The "Line in the Sand" for Dependencies](architecture/mediators.md#the-line-in-the-sand-for-dependencies).
- **Keep layers strictly separated:**
  - **Web layer (`//ios/web/`):** Must not contain Chrome-specific feature
    logic.
  - **Model layer (`model/`):** Owns domain state, persistence, and background
    observation (keyed services, browser agents, tab helpers). Never coupled to
    specific UI implementations.
  - **Coordinators (`coordinator/`):** Own feature lifecycle (`-start` and
    `-stop`), view controller presentation/containment, and dependency wiring.
    Coordinators must not observe models or implement business logic, and must
    never call `[self stop]`. See
    [Coordinators](architecture/coordinators.md#core-guidelines-and-boundaries).
  - **Mediators (`coordinator/`):** Observe models, push UI-ready value types to
    `*Consumer` protocols, provide transient interaction-time decisions via
    `*DataSource` protocols, and execute model mutations via `*Mutator`
    protocols following the
    [Mutation-Observation Cycle](architecture/mediators.md#the-mutation-observation-cycle).
    Never pass model objects to the UI layer. See
    [Mediators](architecture/mediators.md#guidelines-summary).
  - **View Controllers and Views (`ui/`):** Manage layout and user interaction
    with zero model-layer imports.
- **Follow UI command and handler conventions:**
  - Create shared command handlers via
    `HandlerForProtocol(dispatcher, FooCommands)`. Do not pass handlers into
    objects that already have a `Browser*`.
  - Name handler properties and ivars `<name>Handler` (dropping the `Commands`
    suffix, e.g., `id<PageInfoCommands> pageInfoHandler`), declare them `weak`,
    never name them `dispatcher` or bare `handler`, and never combine multiple
    protocols into one property (`id<FooCommands, BarCommands>`). See
    [Naming Command Handlers](architecture/commands.md#naming-command-handlers).
  - For UI actions internal to a single feature, define a local command protocol
    in `ui/` and pass the coordinator directly as the handler without
    `CommandDispatcher`. See
    [Shared vs. Local Command Protocols](architecture/commands.md#shared-vs-local-command-protocols).
- **Avoid forwarding ("middle man") protocols:** Do not create a custom delegate
  protocol on a subview or helper object solely for its owner in the same layer
  to forward the call to a command handler or mutator; pass the handler or
  mutator directly to the child object. See
  [Commands vs. Other Protocol Patterns](architecture/commands.md#commands-vs-other-protocol-patterns).
- **Present simple confirmation alerts from the view controller:** When an alert
  or action sheet confirms a user action for a view controller that is already
  on screen, present the `UIAlertController` directly from the view controller
  and route the confirmed action through its mutator or command handler rather
  than creating a child alert coordinator. See
  [Core Guidelines and Boundaries](architecture/coordinators.md#core-guidelines-and-boundaries).

______________________________________________________________________

## Directory Structure and `DEPS`

### Feature Subdirectories

Feature code under `//ios/chrome/browser/<feature_name>/` is split by
architectural layer (see
[Directory Structure and Dependencies](architecture/coordinators.md#directory-structure-and-dependencies)):

- `model/`: Model-layer classes (keyed services, tab helpers, browser agents).
- `coordinator/`: Coordinator-layer classes (coordinators and mediators).
- `ui/`: Views, view controllers, and shared protocol/item headers (`*Consumer`,
  `*DataSource`, `*Mutator`, local `*Commands`) imported by both `ui/` and
  `coordinator/`. `ui/` must never depend on `coordinator/` or `model/`.

Additional subdirectories (such as `public/` or `test/`) may be created for
constants, feature flags, or EarlGrey test helpers that sit outside the three
layers.

> **Legacy `ui_bundled/` directories:** Older features sometimes combine `ui/`
> and `coordinator/` inside a single `ui_bundled/` directory. `ui_bundled/` must
> always be treated as a **transitional state**:
>
> - **Never create a `ui_bundled/` directory** for a new feature.
> - If you are actively working on code in an existing `ui_bundled/` directory,
>   include a plan to factor that code into proper `coordinator/` and `ui/`
>   directories as part of your work.

### `DEPS` Files

`DEPS` files enforce layering and cross-feature dependency boundaries. If an
`#import` causes a `DEPS` violation, verify first that the dependency does not
violate architectural layering (for example, `ui/` importing from `model/`) and
that both files live in the right directory. When a dependency is legitimate:

- Add a directory-level rule in `DEPS` if the entire directory conceptually
  depends on the target directory.
- Add a `specific_include_rules` entry if only a single file needs the import
  (for example, a coordinator importing a child feature's coordinator header).

______________________________________________________________________

## Objective-C++ and C++ Best Practices

### Namespaces and Symbol Prefixes

- **No prefixes or namespaces in `//ios/chrome/`:** Because `//ios/chrome/` is
  the top-level application layer and is not linked as a library into other
  products, do **not** prefix Objective-C classes/protocols and do **not** put
  C++ code in named namespaces—even when implementing an interface from
  `//components/<foo>/` (write `class FooClientIOS : public foo::FooClient` in
  the global namespace; see
  [Chromium C++ guidance on named namespaces](/styleguide/c++/c++-dos-and-donts.md#named-namespaces)).
  Organize code by directory and clear naming instead.
- **Use prefixes and namespaces in lower layers:**
  - `//ios/web/` uses the `CRW` prefix for Objective-C symbols (e.g.,
    [`CRWWebStateObserver`](/ios/web/public/web_state_observer_bridge.h)) and
    `namespace web` for C++.
  - `//components/<foo>/` and `//ios/components/<foo>/` use `namespace <foo>`
    matching the directory name.

### Headers, Imports, and Declaration Order

- **Use `#import` in `.mm` files:** Never use `#include` in Objective-C++
  (`.mm`) files, even for C or C++ headers; always use `#import`. (Enforced by
  [`PRESUBMIT.py`](PRESUBMIT.py).)
- **Forward-declare in headers:** Unlike the Google Objective-C Style Guide,
  Chromium guidance requires forward-declaring types in headers to minimize
  `#import` fan-out and keep build times fast (see
  [Minimize Code in Headers](/styleguide/c++/c++-dos-and-donts.md#minimize-code-in-headers)).
  In `.h` files, forward-declare classes and protocols (`@class`, `@protocol`,
  `class`) whenever the full definition is not needed (such as for pointer/`id`
  parameters, properties, and return types), and `#import` the full headers in
  the `.mm` file. (You must still `#import` the header when subclassing,
  conforming to a protocol in `@interface`, or using a C++ value type or type
  alias.)
- **Use umbrella headers for system frameworks:** Import system frameworks via
  their umbrella header (`#import <UIKit/UIKit.h>`,
  `#import <Foundation/Foundation.h>`), never individual framework sub-headers,
  to preserve clang module optimizations. (Enforced by
  [`PRESUBMIT.py`](PRESUBMIT.py).)
- **Header declaration order:** Inside every `@interface` in a header file,
  order declarations as mandated by the
  [Google Objective-C Style Guide](https://google.github.io/styleguide/objcguide.html#method-declarations):
  1. `@property` declarations
  2. Class methods (convenience constructors first)
  3. Initializers (`-init...`)
  4. Instance methods

### Properties and Instance Variables

- **Avoid private `@property` wrappers around ivars:** Do not declare private
  `@property`s in class extensions merely to hold internal state; synthesized
  getters and setters bloat binary size (see the
  [Google Objective-C Style Guide](https://google.github.io/styleguide/objcguide.html#avoid-redundant-property-access)).
- **Avoid redundant property access:** Avoid repeatedly calling the same
  property in a method. Instead, use a local variable to store the value and
  reuse it over the rest of the method (see the
  [Google Objective-C Style Guide](https://google.github.io/styleguide/objcguide.html#avoid-redundant-property-access)).
- **Declare ivars in `@implementation`:** Place instance variable declarations
  inside the `@implementation MyClass { ... }` block in the `.mm` file rather
  than in `@interface` or class extensions.
- **Never dereference `->` on weak pointers:** Never access an ivar via `->` on
  `weakSelf` (or any potentially `nil` pointer):
  ```objectivec
  // BAD: Crashes with a null-pointer dereference if `weakSelf` is nil.
  weakSelf->_myIvar;
  ```
  Call a helper method on `weakSelf` instead (which is a safe no-op when
  `weakSelf` is `nil`).

### Managing C++ Lifetimes and Pointers

- **C++ class and struct members:**
  - Prefer an inline value member (`T` or `std::optional<T>`) over heap
    allocation when the object's lifetime matches the owning class. Otherwise,
    use `std::unique_ptr<T>` for single ownership.
  - For unowned references, use `const raw_ref<T>` (from
    `base/memory/raw_ref.h`) when the reference can never be null, `raw_ptr<T>`
    (from `base/memory/raw_ptr.h`) when it may be null or reassigned, and
    `base::WeakPtr<T>` when the pointee may be destroyed asynchronously.
- **Function and method parameters:** Pass unowned objects as raw `T*` or `T&`
  parameters, never `raw_ptr<T>`, `raw_ref<T>`, or `base::WeakPtr<T>`.
  (`raw_ptr<T>` and `raw_ref<T>` are only for member variables and ivars;
  `base::WeakPtr<T>` should only be a parameter of a lambda or free function
  bound into an asynchronous callback when the `T` may be destroyed before the
  callback runs.)
- **Objective-C ivars and properties pointing to C++ objects:**
  - For **owned** C++ objects (such as observer bridges and
    `base::ScopedObservation` instances), store them in `std::unique_ptr<T>`
    ivars in `@implementation` and `.reset()` them during deterministic teardown
    (see
    [Deallocation and Deterministic Cleanup](#deallocation-and-deterministic-cleanup)).
  - For **unowned** C++ objects (such as `PrefService*`, `WebStateList*`, or
    keyed services), store them in `raw_ptr<T>` (or `base::WeakPtr<T>`) ivars in
    `@implementation` (bare `T*` ivars are legacy usage) and explicitly set them
    to `nullptr` during teardown so they do not dangle while the Objective-C
    object awaits deallocation.
  - When exposing an unowned C++ pointer as an Objective-C `@property`, declare
    the property with a bare `T*`
    (`@property(nonatomic, assign) CppBar* cppBar;`) and implement explicit
    getter and setter methods backed by a `raw_ptr<T>` or `base::WeakPtr<T>`
    ivar:
    ```objectivec
    @implementation Foo {
      raw_ptr<CppBar> _cppBar;
    }

    - (void)disconnect {
      _cppBar = nullptr;
    }

    - (CppBar*)cppBar {
      return _cppBar.get();
    }

    - (void)setCppBar:(CppBar*)cppBar {
      _cppBar = cppBar;
    }
    @end
    ```

### Null Checks and Assertions (`CHECK`)

- **Do not use Objective-C nullability annotations:** Do not use `nonnull`,
  `nullable`, `_Nonnull`, or `_Nullable` in `//ios/` outside
  `//ios/web_view/public/` and `//ios/third_party/`. (Enforced by
  [`PRESUBMIT.py`](PRESUBMIT.py).)
- **Enforce invariants with `CHECK` (do not use `DCHECK`):** Following
  Chromium's
  [`CHECK()`, `DCHECK()`, and `NOTREACHED()` guidance](/styleguide/c++/checks.md),
  assert preconditions and invariants with `CHECK` rather than silently hiding
  broken invariants. Because Chrome on iOS has a small pre-stable population,
  choose between a fatal `CHECK` and `base::NotFatalUntil` based on whether
  execution can safely continue if the invariant is violated:
  - **When continuing cannot be made safe (unconditional crash or memory
    hazard):** Use a bare `CHECK(cond)` (for example, immediately before
    dereferencing a required pointer when there is no sensible recovery path).
  - **When execution can safely recover:** Use
    `CHECK(cond, base::NotFatalUntil::M...)` followed by graceful fallback
    handling (such as an early return) so that an unexpected violation in the
    wild generates a non-fatal crash report without crashing users or forcing a
    release respin:
    ```objectivec
    CHECK(item, base::NotFatalUntil::M161);
    if (!item) {
      return;
    }
    ```
    Check [`//chrome/VERSION`](/chrome/VERSION) for the current milestone `M`
    (the `MAJOR` value) and set the milestone at least **3–4 milestones in the
    future** (`M + 3` or `M + 4`; never copy an older milestone from surrounding
    code) so the check reaches Stable and collects telemetry for at least one
    full release before turning fatal.
  - **Do not use `DCHECK`:** Never add new `DCHECK`s; use `CHECK` instead. (The
    only exception is `DCHECK_CALLED_ON_VALID_SEQUENCE()` /
    `DCHECK_CALLED_ON_VALID_THREAD()`, which have no `CHECK` equivalent.)

### Deallocation and Deterministic Cleanup

Objective-C object deallocation timing (`-dealloc`) is non-deterministic: UIKit
animations, presentation controllers, blocks, and autorelease pools often keep
objects alive after a feature is dismissed. **Never rely on `-dealloc` for
cleanup required for correctness** (such as removing C++ observers or stopping
command dispatch).

Instead, tear down deterministically:

- Coordinators stop child coordinators, unregister command targets, disconnect
  mediators, and dismiss view controllers in `-stop` (see
  [Deterministic Teardown in `-stop`](architecture/coordinators.md#deterministic-teardown-in-stop)).
- Mediators reset C++ observer bridges and clear unowned C++ pointers in
  `-disconnect` (see
  [Teardown and `-disconnect`](architecture/mediators.md#teardown-and-disconnect)).

### Blocks

Blocks capture and strongly retain referenced Objective-C objects while leaving
C++ pointers unmanaged. Follow these rules to avoid retain cycles and
use-after-free bugs:

- **Use blocks only when required by an API.** Do not use local blocks in place
  of private helper methods.
- **Keep blocks to a single method call on `weakSelf`:**
  ```objectivec
  __weak __typeof(self) weakSelf = self;
  [service performOperationWithCompletion:^(BOOL success) {
    [weakSelf operationDidCompleteWithSuccess:success];
  }];
  ```
  Moving the body into a private method avoids `weakSelf`/`strongSelf` dance
  bugs, prevents accidental ivar capture, and produces clear stack traces.
- **Never reference `self` (or ivars) directly inside a block:** Referencing
  `self` or `_ivar` inside a block strongly retains `self`. Always capture
  `__weak __typeof(self) weakSelf = self;`.
- **Never capture raw C++ objects or `this` in a block:** Blocks do not manage
  C++ lifetimes, and calling a C++ member function in a block implicitly
  captures raw `this`.
- **Do not use blocks in C++ classes:** In a C++ class, bind a
  `base::OnceCallback` / `base::RepeatingCallback` (using `base::WeakPtr<T>`)
  and convert it with `base::CallbackToBlock(...)` when calling an API that
  requires a block (see below).
- **Name block parameters for their role (`callback` vs. `completion`, never
  `handler`):**
  - Use **`callback`** when the block returns information to the caller (for
    example, the result of an asynchronous network request or query).
  - Use **`completion`** when the block signals that an asynchronous operation
    has finished (such as an animation or view presentation).
  - Choose another specific, descriptive name when neither role applies.
  - **Never name a block parameter `handler` (or `*Handler`):** In Chrome on
    iOS, "handler" is reserved for
    [UI command handlers](architecture/commands.md#naming-command-handlers).
- **Converting between blocks and C++ callbacks:**
  - **Avoid unnecessary conversions:** Only convert at the boundary where you
    have a `base::OnceCallback` / `base::RepeatingCallback` and must call an
    Objective-C API that requires a block, or where you have a block and must
    call an API that requires a callback.
  - **Callback to block (`base::CallbackToBlock`):** Use
    `base::CallbackToBlock(std::move(callback))` (from
    [`base/functional/callback_helpers.h`](/base/functional/callback_helpers.h)),
    which returns an Objective-C block that wraps the callback. Note that blocks
    do not enforce the semantics of the underlying callback, and reusing a block
    that wraps a `base::OnceCallback` is still a programming error.
  - **Block to callback (`base::BindOnce` / `base::BindRepeating`):** When
    passing an Objective-C block into an API that expects a `base::OnceCallback`
    or `base::RepeatingCallback`, wrap the block with `base::BindOnce(block)` or
    `base::BindRepeating(block)`. In this direction, the wrapping callback
    enforces its own invocation semantics (single-use for `OnceCallback`,
    repeatable for `RepeatingCallback`) on the underlying block.

### Type Deduction (`auto`), `id`, and Literals

- **When to use `auto`:** Following the
  [Google C++ Style Guide on type deduction](https://google.github.io/styleguide/cppguide.html#Type_deduction),
  use `auto` only when it improves readability or safety:
  - Local block variables in Objective-C++:
    ```objectivec
    auto completion = ^(BOOL success, NSError* error) { ... };  // GOOD
    ```
  - Range-based `for` loops and iterators in C++:
    ```cpp
    for (const auto& report : reports) { ... }  // GOOD
    ```
  - `std::make_unique<T>()` (and similar casts/factories) where the type is
    already written on the right-hand side:
    ```cpp
    auto sites = std::make_unique<ntp_tiles::MostVisitedSites>(...);  // GOOD
    ```
- **When NOT to use `auto`:** Never use `auto` for Objective-C `alloc`/`init`
  expressions or to hide return types that aren't visible in the statement:
  ```objectivec
  auto item = [[MyItem alloc] init];  // BAD
  MyItem* item = [[MyItem alloc] init];  // GOOD
  ```
- **Use concrete types instead of `id`:** Unlike `auto`, `id` disables static
  type checking. Reserve `id` for protocol-qualified references
  (`id<FooConsumer>`, `id<FooCommands>`). Always write the concrete class type
  when known.
- **Boxed `BOOL` literals:** Write `@YES` and `@NO`, never `@(YES)` or `@(NO)`.
  (Enforced by [`PRESUBMIT.py`](PRESUBMIT.py).)

### Enumerations (`enum class` vs. `NS_ENUM`)

Because all Objective-C code in `//ios/` is Objective-C++ (`.mm`), prefer C++
scoped enums (`enum class`) over `NS_ENUM`:

```objectivec
// BAD:
typedef NS_ENUM(NSInteger, DownloadState) {
  DownloadStateInProgress,
  DownloadStateComplete,
};

// GOOD:
enum class DownloadState {
  kInProgress,
  kComplete,
};

// OK when an explicit underlying integer type is required:
enum class DownloadState : NSInteger {
  kInProgress,
  kComplete,
};
```

<!--
When an API requires an integer value (such as legacy `TableViewModel` section
or item type identifiers), still use `enum class` and convert explicitly with
`std::to_underlying(value)` (from `<utility>`).
-->

### Protocol Naming and Design Conventions

Chrome on iOS has established patterns for protocol-defined interfaces, and
these protocol naming and design conventions carry the same weight as our class
naming conventions. Whenever you define a protocol, design and name it to fit
one of these established roles rather than inventing ad-hoc conventions:

| Protocol Role        | Naming Suffix | Passes `self` as Sender? | Return Types                    | Declared In                                    | Reference                                                                               |
| -------------------- | ------------- | ------------------------ | ------------------------------- | ---------------------------------------------- | --------------------------------------------------------------------------------------- |
| Consumer             | `*Consumer`   | Never                    | `void`                          | Feature `ui/`                                  | [Consumer Protocols](architecture/mediators.md#consumer-protocols)                      |
| Consumer Data Source | `*DataSource` | Never                    | UI decision / config value type | Feature `ui/`                                  | [On-Demand Data](architecture/mediators.md#on-demand-data-from-the-mediator)            |
| Mutator              | `*Mutator`    | Never                    | `void` (almost always)          | Feature `ui/`                                  | [Model Updates](architecture/mediators.md#model-updates-through-the-mediator)           |
| Shared UI Command    | `*Commands`   | Never                    | `void`                          | `//ios/chrome/browser/shared/public/commands/` | [UI Commands](architecture/commands.md)                                                 |
| Local UI Command     | `*Commands`   | Never                    | `void`                          | Feature `ui/`                                  | [Shared vs. Local Commands](architecture/commands.md#shared-vs-local-command-protocols) |
| UIKit-style Delegate | `*Delegate`   | Always (1st parameter)   | `void` or query value           | Alongside delegating class                     | [Coordinator Delegates](architecture/coordinators.md#coordinator-delegates)             |
| Observer Bridge      | `*Observing`  | Usually                  | `void` (`@optional` methods)    | Model / bridge header                          | [Observing Model Objects](architecture/mediators.md#observing-model-objects)            |
| Audience             | `*Audience`   | Never                    | `void` (one-to-one, one-way)    | Alongside actor class                          | See below                                                                               |

- **Delegate (`*Delegate`) protocols:** Do not use "delegation" for arbitrary
  plumbing. Because UIKit uses delegates pervasively, it is easy to fall into
  the habit of naming any helper protocol `*Delegate` and structuring it however
  is convenient. Don't do this: delegation is a specific design pattern that
  must be used intentionally:
  - **A delegate must always be optional:** The `delegate` property can be `nil`
    and is set after initialization; the delegating object must still function
    (even if only trivially) when no delegate is set. If the object cannot
    function without the collaborator, it is not a delegate (the relationship
    may actually be a `*Mutator`, `*DataSource`, `*Commands` handler, or
    `*Audience`).
  - Delegates must be held weakly by the delegating object.
  - Every method in a delegate protocol must pass the delegating object (`self`)
    as its first parameter, and should not pass additional parameters that are
    already available from the delegating object's public API.
  - Delegate protocols do not include `@property` declarations, since the
    delegating object cannot be passed into a property getter.
- **Observer (`*Observing`) protocols:** Used when bridging C++ observer classes
  to Objective-C (see
  [Observing Model Objects](architecture/mediators.md#observing-model-objects));
  they follow similar conventions to delegate protocols, and their methods are
  usually marked `@optional`.
- **Audience (`*Audience`) protocols:** Used when an object (the *actor*)
  informs one collaborator (the *audience*) of changes without coupling their
  implementations:
  - Audience methods do not pass the actor (`self`) as a parameter, and do not
    return values (communication is strictly one-way).
  - The audience is held weakly, may be required (non-`nil`) and passed at
    initialization, and is strictly one-to-one (use observation for one-to-many
    relationships).
  - Before creating an audience protocol, make sure the relationship isn't
    better expressed as a `*Mutator` (a view controller requesting model
    updates) or a `*Commands` protocol (requesting a UI change).
- **Use Objective-C naming patterns for event-based methods:** Event-driven
  methods in Objective-C (especially in observer, delegate, and audience
  protocols) must use standard Objective-C `will`/`did`/`should` naming
  (`-thingWillClose:`, `-thingDidClose:`), **never** the C++/Java/JavaScript
  `on*` prefix style (`-onThingClosed:`). When bridging a C++ `Observer` whose
  methods are named `OnThingClosed()`, translate the Objective-C selector to
  idiomatic Objective-C.
- **Avoid generic suffixes:** Never name a protocol `*Protocol` or (if it can be
  avoided) `*Helper`. Use one of the established role suffixes above, or choose
  a name that narrowly describes the job performed by the conforming object.

### Categories and Class Extensions

- **Class extensions (`@interface Foo ()`):** Place in the `.mm` file above
  `@implementation` to declare private protocol conformances (such as command or
  observer protocols).
- **Categories (`@interface Foo (Bar)`):** Declare in separate files
  (`foo+bar.h`, `foo+bar.mm`).
- **Prefix category methods on system classes:** Every category method added to
  an Apple framework class (`UIView`, `NSString`, etc.) must be prefixed with
  `cr_` in `//ios/chrome/` or `crw_` in `//ios/web/` to prevent runtime selector
  collisions.
- **Subclasses vs. Tests:** Use a `+Subclassing` (or `+Protected`) category
  header to expose methods intended only for subclasses. Avoid `+Testing`
  categories that expose private internals to tests—and **never** redeclare
  private methods in an ad-hoc `+Testing` category inside a `*_unittest.mm`
  file; test through the public API and injected dependencies instead (see, for
  example,
  [Unit Testing Mediators](architecture/mediators.md#unit-testing-mediators)).

### Instantiating View Hierarchies in Initializers

The
[Google Objective-C Style Guide](https://google.github.io/styleguide/objcguide.html#avoid-messaging-the-current-object-within-initializers-and-dealloc)
forbids messaging `self` in `-init`. Chrome on iOS allows one **narrow
exception**: a `UIView` subclass may call private helper methods on `self` in
`-init` if and only if those methods exclusively construct and lay out its
subview hierarchy.

______________________________________________________________________

## Chromium and iOS Platform Abstractions

When both Apple Foundation/UIKit and Chromium (`//base/` or `//components/`)
provide abstractions for the same concept, **prefer the Chromium abstraction**
for model, state, time, and concurrency, and **prefer native iOS APIs** for UI
and system integration:

| Need                        | Prefer                                                      | Avoid in New Code                                    |
| --------------------------- | ----------------------------------------------------------- | ---------------------------------------------------- |
| Persistent state / settings | `PrefService` (profile prefs or `localState`)               | `NSUserDefaults`                                     |
| Timestamps & durations      | `base::Time`, `base::TimeTicks`, `base::TimeDelta`          | `NSDate`, `NSTimeInterval` (except UIKit animations) |
| Asynchronous / bg tasks     | `base::SequencedTaskRunner`, `PostTask`, `PostTaskAndReply` | `dispatch_async` (except when required by iOS APIs)  |
| View animations             | UIKit animations (`UIView`, `UIViewPropertyAnimator`)       | `gfx::Animation`                                     |

### Storing State: Use `PrefService`, Not `NSUserDefaults`

New feature code must store persistent state in `PrefService` (profile prefs or
application `localState`), not `[NSUserDefaults standardUserDefaults]` (enforced
by [`PRESUBMIT.py`](PRESUBMIT.py)):

1. `[NSUserDefaults standardUserDefaults]` is process-global and cannot
   partition data per `ProfileIOS` for multi-identity support.
2. Cross-platform Chromium components read and write preferences via
   `PrefService`.
3. `NSUserDefaults` has no hermetic test double and persists across test runs,
   causing cross-test pollution and flake.

*Exceptions:* `NSUserDefaults` is used only when required by the platform or
execution environment:

- **App Group shared defaults (`app_group::GetGroupUserDefaults()`):** Sharing
  data across processes with iOS app extensions (such as WidgetKit widgets, the
  Credential Provider Extension, or Share/Open extensions), which cannot access
  the main app's `PrefService`.
- **Early startup before `PrefService` initialization:** Reading or caching
  state required during early startup or crash reporting before
  `ApplicationContext` and `PrefService` are loaded.
- **OS-level and experimental settings:** Reading iOS Settings / Experimental
  settings (`Experimental.plist` via `system_flags`) or managed enterprise app
  configuration.

### Measuring Time: Use `base::Time`, `base::TimeTicks`, and `base::TimeDelta`

- Use **`base::TimeDelta`** constants and constructors (`base::Seconds(5)`,
  `base::Minutes(10)`, `base::Days(7)`) instead of raw `NSTimeInterval` or
  integer constants (which risk unit mismatches at the call site):
  ```cpp
  // BAD: Easy to mismatch units (`base::Minutes(kTimeoutSeconds)`).
  constexpr int kTimeoutSeconds = 5;
  // GOOD: Units are part of the type at definition.
  constexpr base::TimeDelta kTimeout = base::Seconds(5);
  ```
- Use **`base::TimeTicks::Now()`** to measure elapsed durations within a process
  (such as histogram timing), because `TimeTicks` is monotonic.
- Use **`base::Time::Now()`** for wall-clock timestamps persisted to disk, sent
  over the network, or shown to the user.
- *Exception:* Short constants passed directly to UIKit animation APIs may use
  `NSTimeInterval`.

______________________________________________________________________

## Concurrency Best Practices

*Nomenclature:*

- **Sequence:** A thread, GCD queue, or Chromium `SequencedTaskRunner`.

- **Asynchronous:** Code scheduled to run on a future turn of a runloop or
  sequence rather than synchronously on the current call stack.

- **Parallel:** Code running on a different sequence from the main sequence.

- **Multithreaded:** An object accessed concurrently from multiple sequences.

- **Do not write multithreaded objects:** Objects in Chrome on iOS are bound to
  the sequence on which they are created (enforced with `SEQUENCE_CHECKER`). If
  you find yourself reaching for locks, mutexes, `@synchronized`, or `atomic`
  Objective-C properties, stop and redesign around sequence-bound objects and
  task posting. All calls across the mediator/UI boundary must run on the main
  sequence (see
  [Sequence Guarantees](architecture/mediators.md#sequence-guarantees)).

- **Use Chromium task runners:** Prefer `base::SequencedTaskRunner` and
  `PostTask` / `PostDelayedTask` over `dispatch_async` unless an iOS system API
  requires a `dispatch_queue_t`.

- **Comment every asynchronous dispatch:** Any `PostTask` or `dispatch_async`
  call must have a comment explaining *why* the work is asynchronous.

- **Move/copy inputs and reply with outputs (`PostTaskAndReply`):** When running
  background work, pass owned/copied inputs into a stateless callback that
  touches no sequence-bound objects (`self` or `this`), and deliver outputs back
  to the originating sequence via `PostTaskAndReply` or
  `PostTaskAndReplyWithResult`:

  ```objectivec
  // Deletes `paths` on a background sequence and runs `completion` on the
  // originating sequence when finished.
  - (void)deletePaths:(std::vector<base::FilePath>)paths
           completion:(base::OnceClosure)completion {
    // Perform file I/O in the background to avoid blocking the main sequence.
    _taskRunner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            [](std::vector<base::FilePath> paths) {
              for (const base::FilePath& path : paths) {
                base::DeletePathRecursively(path);
              }
            },
            std::move(paths)),
        std::move(completion));
  }
  ```

- **Never use `PostTask` or `dispatch_async` as a proxy for completion:** Do not
  hop to the next runloop turn hoping an animation, view presentation, network
  fetch, or disk operation has finished. Runloop-turn timing is coincidental and
  causes race conditions and flaky tests. Always chain work from the explicit
  completion block, `base::OnceCallback`, delegate callback, or observer
  notification:

  ```objectivec
  // BAD: Relying on the next runloop turn after starting async work.
  [self presentViewController:viewController animated:YES completion:nil];
  dispatch_async(dispatch_get_main_queue(), ^{
    [self doWorkAfterPresentation];
  });

  // GOOD: Using the explicit completion handler.
  __weak __typeof(self) weakSelf = self;
  [self presentViewController:viewController
                     animated:YES
                   completion:^{
                     [weakSelf doWorkAfterPresentation];
                   }];
  ```

______________________________________________________________________

## UI and Assets Best Practices

- **Use semantic colors:** Never use standard UIKit system colors (such as
  `UIColor.redColor` or `UIColor.systemBlueColor`; only `whiteColor`,
  `blackColor`, and `clearColor` are permitted) or hardcoded RGB values. Always
  use semantic color names from
  [`//ios/chrome/common/ui/colors/semantic_color_names.h`](/ios/chrome/common/ui/colors/semantic_color_names.h)
  via `[UIColor colorNamed:k...]` so Light and Dark Mode work automatically. New
  `.colorset` assets must be added to `//ios/chrome/common/ui/colors/`.
  (Enforced by [`PRESUBMIT.py`](PRESUBMIT.py).)
- **Prefer SF Symbols and small vector assets:**
  - Use the `Symbol` enum and symbol helpers in
    [`//ios/chrome/browser/shared/ui/symbols/symbols.h`](/ios/chrome/browser/shared/ui/symbols/symbols.h)
    (`SymbolWithPointSize()`, `SymbolTemplateWithPointSize()`,
    `SymbolWithConfiguration()`) for icons whenever possible.
  - Never add large (> 100pt) PDF or SVG vector files to `.imageset`
    directories; Apple's `actool` rasterizes them into uncompressed 32-bit ARGB
    bitmaps at compile time, severely bloating `Assets.car`. Use `@2x` and `@3x`
    PNGs for large illustrations instead. (Enforced by
    [`PRESUBMIT.py`](PRESUBMIT.py).)
  - Use `UIGraphicsImageRenderer` for offscreen bitmap rendering, never the
    deprecated `UIGraphicsBeginImageContext` APIs. (Enforced by
    [`PRESUBMIT.py`](PRESUBMIT.py).)
- **Use Auto Layout and Dynamic Type:**
  - Build layouts with Auto Layout constraints and layout guides, using the
    helpers in
    [`//ios/chrome/common/ui/util/constraints_ui_util.h`](/ios/chrome/common/ui/util/constraints_ui_util.h)
    (`AddSameConstraints()`, `AddSameConstraintsWithInsets()`, etc.) to reduce
    boilerplate.
  - Support Dynamic Type by using `[UIFont preferredFontForTextStyle:...]` (or
    `UIFontMetrics`) and setting `adjustsFontForContentSizeCategory = YES` on
    labels, buttons, and text views.

______________________________________________________________________

## Testing Best Practices

- **Test at the lowest appropriate level:**
  - **Unit tests (`*_unittest.mm`):** Required for mediators and model classes
    (tab helpers, browser agents, services), and recommended for view
    controllers with non-trivial state or layout logic. Inject fake or test
    model services and mock/fake protocols through public initializers and
    properties rather than exposing private implementation details (see, for
    example,
    [Unit Testing Mediators](architecture/mediators.md#unit-testing-mediators)).
  - **EarlGrey tests (`*_egtest.mm`):** Use for end-to-end integration and user
    journeys across UI and model layers.
- **Use Objective-C GTest macros in unit tests:** Prefer `EXPECT_NSEQ(a, b)` and
  `EXPECT_NSNE(a, b)` over `EXPECT_TRUE([a isEqualToString:b])` so test failures
  print both actual and expected Objective-C objects. (Enforced by
  [`PRESUBMIT.py`](PRESUBMIT.py).)
- **Suppressing failing or flaky tests:**
  - **XCTest and EarlGrey tests (`*_egtest.mm`):** Do **not** use `DISABLED_` or
    `FLAKY_` method prefixes or hard-coded runtime/compile-time skips in test
    code to suppress failing or flaky tests. Instead, add an entry to
    [`//ios/testing/test_expectations.txt`](/ios/testing/test_expectations.txt)
    linking the `crbug.com/123456` issue, any platform/configuration tags (such
    as `[ iPad Simulator ]` or `[ Device ]`), the test name, and the expected
    outcomes (`[ Failure ]`, `[ Crash ]`, `[ Skip ]`, or `[ Pass Failure ]` for
    flaky tests). Centralizing suppressions in `test_expectations.txt` scopes
    expectations to specific configurations and ensures CQ flags when an
    expected-failing test starts passing or changes its failure mode. In-test
    checks (such as `@available` guards) should only be used when required for
    compilation or when `test_expectations.txt` cannot express the condition.
  - **GTest unit tests (`*_unittest.mm`):** Never use the `FLAKY_` prefix on
    unit tests (enforced by [`PRESUBMIT.py`](PRESUBMIT.py)); if a unit test is
    flaky or failing, disable it with the `DISABLED_` prefix and a
    `TODO(crbug.com/123456)` comment.
  - **Closing test bugs:** Never close a bug for a disabled or suppressed test
    without re-enabling the test, removing its `test_expectations.txt` entry or
    `DISABLED_` prefix, and deleting any associated `TODO`s.
- **EarlGrey test conventions:**
  - Do not override `-tearDown` in `*_egtest.mm` files; register cleanup with
    `[self addTeardownBlock:^{ ... }]` or override `-tearDownHelper` to keep
    test cases hermetic. (Enforced by [`PRESUBMIT.py`](PRESUBMIT.py).)
  - To verify page navigation in EarlGrey tests, use
    `[ChromeEarlGrey waitForWebStateVisibleURL:...]` rather than asserting on
    `OmniboxText`. (Enforced by [`PRESUBMIT.py`](PRESUBMIT.py).)
