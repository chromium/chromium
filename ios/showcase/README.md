# Chrome for iOS Showcase development app

This folder holds the code for a dev-only, standalone app for Chrome for iOS
development. It showcases (and is restricted to) UI elements used and developed
for Chrome for iOS.
Goals for this app are to be as simple, as easy to maintain, and as useful as
possible.

## Detailed Design

This is a standalone app that is built by the bots, much like `ios_web_shell`
and `chrome_clean_skeleton`.

### src/ios/showcase

```
ios/chrome/app:app
ios/chrome/app:chrome_clean_skeleton
ios/showcase:showcase
ios/showcase:all_tests
ios/web/shell:ios_web_shell
```

Much like `experimental/`, the app lives in its own folder. It depends on
Chrome UI code for the production code, but the architecture of the Showcase app
is separate.

### Language and build

* **Objective-C++** needed to interact with the view controller classes likely
  to be in Objective-C++ as well.

* **ARC**

* **GN and Ninja.** The showcase and `showcase:all_tests` targets are linked to
  `gn_all`.

### Features

* **Class and use cases**
Table of use cases demoing view controller classes under different sets of entry
data. There can be several use cases for the same class so each gets an entry.
Cells on homepage display the use case name and the class name of the view
controller they link to.

* **Search**
Search box at the top to filter the cells based on the class and use case name.

* **Simple, tailor-made coordinators**
Coordinators handle flow, so Showcase shouldn't host any of Chrome's
coordinators. Instead, simple and lightweight coordinators will be used in
Showcase. They will handle the initialization of a production view controller,
and setup all mock data needed to display this view controller.

### Isolation from Chrome

View controllers in the New Architecture shouldn't need the BrowserProfile, etc.
So mocking the inputs and outputs of a single VC should be easy and not require
the core of Chrome.

### Non-goals

* ~~Table views with TableViewModel/CollectionViewModel?~~
    * We want to make the app as small and simple as possible. The fewer
      dependencies the better.

* ~~Can we configure from within the app what these inputs and outputs are?~~
    * Not desirable. Too complex for what we want to achieve.

## How do I showcase a Chrome UI feature?

The following nomenclature is used:

* A **feature** is a Chrome UI feature such as a full-screen or partial-screen
  view controller, a cell, a view, or a UI control (such as a custom button).

### To add a feature to Showcase

1. Add an entry to the Showcase configuration file, which requires 3 pieces of
   information:

    1. **_ClassForDisplay_** - The UI feature that you want to showcase, such
       as a SettingsViewController, or AccountCell, or TabContainer. It appears
       as the title of the cell in Showcase.

        1. **_ClassForInstantiation_** - The actual class that is instantiated
           by Showcase. This is either a view controller or a custom
           coordinator. (*More information below*).

    2. **_UseCase_** - A short description that is helpful to the user about
       what is being showcased.

    ```
    @{
      showcase::kClassForDisplayKey : @"SettingsViewController",
      showcase::kClassForInstantiationKey : @"SettingsCoordinator",
      showcase::kUseCaseKey : @"Main settings screen",
    },
    ```

2. Add the target that contains *ClassForInstantiation* to `showcase/BUILD.gn`.

    ```
    group("features") {
      deps = [
        "//ios/chrome/browser/ui/tools:tools_ui",
        "//ios/showcase/settings",
        "//ios/showcase/tab_bottom",
        "//ios/showcase/tab_grid",
        "//ios/showcase/tab_top",
        "//ios/showcase/uikit_table_view_cell",
        # Insert additional feature targets here.
      ]
    }
    ```

3. Add the target that contains Earl Grey tests (if any) to `showcase/BUILD.gn`.

    ```
    ios_eg_test("ios_showcase_egtests") {
      deps = [
        "//ios/showcase/core:eg_tests",
        "//ios/showcase/tab_grid:eg_tests",
        # Insert additional feature eg_tests targets here.
      ]
    }
    ```

### More on ClassForInstantiation

There are 3 scenarios for adding features to Showcase:

* Directly use the ClassForDisplay when it can be fully initialized with a call
  to `-init`.

    ```
    showcase::kClassForDisplayKey : @"FeatureViewController",
    showcase::kClassForInstantiationKey : @"FeatureViewController",
    ```

* Use a custom coordinator when you need to showcase a view controller that
  requires special initialization or setup (e.g., requires model objects).

    ```
    showcase::kClassForDisplayKey : @"FeatureViewController",
    showcase::kClassForInstantiationKey : @"FeatureCoordinator",
    ```

* Use a custom view controller when you need to showcase a cell, view, or UI
  control.

    ```
    showcase::kClassForDisplayKey : @"FeatureView",
    showcase::kClassForInstantiationKey : @"FeatureViewViewController",
    ```

### How to create a custom coordinator

See example: `SettingsCoordinator - Main settings screen`

1. Create a folder for `/showcase/feature/`.

2. Create `feature_use_case_coordinator.h|mm`.

    * Must conform to the `Coordinator` protocol provided under
      `/showcase/common/`.

    * Must support being initialized with `-init`.

    * In the `-start` method, instantiate and setup your `FeatureViewController`
      with all the mock data necessary to recreate the use case you are
      showcasing. Then the view controller is pushed onto the navigation
      controller.

3. Create `/showcase/feature/BUILD.gn`.

    ```
    source_set("feature) {
      sources = [
        "feature_use_case_coordinator.h",
        "feature_use_case_coordinator.mm",
      ]
      deps = [
        "//ios/showcase/common",
        # Insert target for ClassForDisplay here.
      ]
      frameworks = [ "UIKit.framework" ]
      configs += [ "//build/config/compiler:enable_arc" ]
    }
    ```

### How to showcase a view (not view controller)

See example: `UIKitTableViewCellViewController - UIKit Table Cells`

You will need a glue view controller.

1. Create a folder for `/showcase/feature_view/`.

2. Create `feature_view_view_controller.h|mm`.

    1. Must support being initialized with `-init`.

    2. Add your view to the view controller and set it up the way you want
       (usually in `-viewDidLoad`).

3. Create `/showcase/feature/BUILD.gn`.

    ```
    source_set("feature) {
      sources = [
        "feature_view_view_controller.h",
        "feature_view_view_controller.mm",
      ]
      deps = [
        # Insert target for ClassForDisplay here.
      ]
      frameworks = [ "UIKit.framework" ]
      configs += [ "//build/config/compiler:enable_arc" ]
    }
    ```
