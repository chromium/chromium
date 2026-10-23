---
name: ios/omit-no-op-super-method-overrides
description: In Objective-C subclasses, coordinators, and tests, omit redundant overrides forwarding to super, trivial init implementations or declarations, empty lifecycle methods, and default appConfigurationForTestCase
rubric: readability
severity: should_fix
---

**Rule.** Do not define or retain redundant method overrides, trivial
initializer implementations, or unnecessary declarations in Objective-C and
Objective-C++ classes (such as view controllers, coordinators like
`ChromeCoordinator`, mediators, or test fixtures like `ChromeTestCase` /
`BaseEarlGreyTestCase`) when they introduce no custom behavior, logic, or state
changes. Specifically:

1. **Pass-through method overrides**: Omit overrides that merely forward or
   delegate directly to `[super ...]`, including `UIViewController` lifecycle
   methods (`-viewDidLoad`, `-viewWillAppear:`, `-viewDidAppear:`,
   `-viewWillDisappear:`, `-viewDidDisappear:`, `-viewDidLayoutSubviews`), test
   fixture lifecycle methods (`-setUp`, `-tearDown`, `-tearDownHelper`),
   inherited test methods (`-test...`), and arbitrary pass-through methods (such
   as `-showConfirmationFromSourceView:`, `-shouldDismissOnBackgroundTap`). Rely
   directly on dynamic dispatch and test runner discovery.
2. **Trivial initializer overrides and declarations**: Omit initializers (such
   as `-init` or `-initWith...:`) that merely forward to `[super ...]` with
   identical arguments without performing custom setup or state initialization.
   Additionally, do not explicitly declare
   `- (instancetype)init NS_DESIGNATED_INITIALIZER;` on `NSObject` subclasses
   when `init` is the sole initializer and requires no custom initialization
   logic; inherit the initializer directly from the superclass.
3. **Empty lifecycle overrides**: Omit empty overrides (such as `-start` or
   `-stop` in `ChromeCoordinator` subclasses) when the base class already
   provides a default no-op implementation.
4. **Default test configuration overrides**: In EarlGrey tests
   (`ChromeTestCase`), omit or remove `-appConfigurationForTestCase` overrides
   when removing feature flags, variations, or launch arguments if the method
   only returns a default-constructed `AppLaunchConfiguration` without
   customization; rely on the base class (`BaseEarlGreyTestCase`) default
   implementation instead.

**Why.** In Objective-C, subclasses automatically inherit superclass methods,
initializers, and default behaviors via dynamic message dispatch, and test
runners (such as XCTest and EarlGrey) automatically discover inherited test
methods. Defining redundant overrides that only forward to `[super ...]`,
implementing pass-through `-init` / `-initWith...:` methods, redeclaring
`-init NS_DESIGNATED_INITIALIZER` on `NSObject` subclasses, retaining empty
bodies for base no-op methods (such as `ChromeCoordinator -start` / `-stop`), or
returning an unmodified default `AppLaunchConfiguration` in
`-appConfigurationForTestCase` adds visual clutter and dead code. It introduces
unnecessary boilerplate, misleads readers into expecting custom behavior, and
increases maintenance churn when method signatures or superclass contracts
change.

## Examples

Bad:

```cpp
// Redundant declaration and trivial initializer:
@interface MyMediator : NSObject
- (instancetype)init NS_DESIGNATED_INITIALIZER;
@end
- (instancetype)init {
  return [super init];
}

// Redundant super forwarding and empty coordinator lifecycle:
- (void)viewDidLoad {
  [super viewDidLoad];
}
- (void)stop {}

// Redundant EarlGrey test configuration override:
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  return config;
}
```

Good:

```cpp
// Inherit NSObject's init directly; omit trivial override and declaration:
@interface MyMediator : NSObject
@end

// Omit -viewDidLoad and -stop when no custom logic or cleanup is needed;
// runtime dispatch invokes superclass methods and coordinator defaults.

// Only implement -appConfigurationForTestCase when customizing flags/args:
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kMyFeature);
  return config;
}
```
