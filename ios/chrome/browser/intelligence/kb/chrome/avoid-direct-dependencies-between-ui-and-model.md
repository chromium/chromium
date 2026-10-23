---
name: ios/avoid-direct-dependencies-between-ui-and-model
description: In ios/chrome/browser, do not allow direct dependencies between UI/coordinator and model layers; relocate shared constants to public/model and define delegates in consumer layer
rubric: design
severity: should_fix
citations:
  - path: ios/chrome/browser/shared/README.md
    heading: Shared folder
    quote: Note that coordinators/mediators have access to all sub-folders, so if you are adding code related to the UI but that coordinators/mediators will also use, the correct folder is ui/.
  - path: ios/chrome/browser/shared/public/README.md
    heading: Shared public folder
    quote: 'This is code that is used by all layers of our architecture or "public" API of a layer (for example: commands as public API of the UI, flags as it is shared by all layers...).'
  - path: docs/dependencies.md
    heading: Diagnosing and Fixing checkdeps Violations
    quote: Can you achieve your goal without adding this new dependency? Reusing existing abstractions is always preferred. ... Ensure this new dependency makes architectural sense and get approval from the code owners of the DEPS file.
---

**Rule.** In `ios/chrome/browser/`, do not allow direct dependencies between the
UI layer (`ui/`, `ui_bundled/`) and the model layer (`model/`), and do not allow
model code or `DEPS` to depend on or import coordinator, mediator, or UI headers
(`coordinator/`, `ui/`, `ui_bundled/`). Specifically:

1. UI targets and source files must not depend on or include `model/`
   directories. In UI protocols and view interfaces, declare a local callback
   alias (`using FooCallback = void (^)(...)`) and forward-declare UI data types
   (e.g., using `@class`) instead of importing model headers or taking model
   build dependencies solely to reuse their callback typedefs.
2. Model code and `DEPS` must not depend on or include headers or build targets
   from `ui/`, `ui_bundled/`, or `coordinator/` directories. Do not punch DEPS
   exceptions or import UI/coordinator headers to reuse protocols or callbacks;
   when model components require callbacks implemented by UI or coordinators,
   define the delegate protocol in the consumer model layer (e.g.,
   `model/<consumer>_delegate.h`) named after the consuming class (e.g.,
   `<Consumer>Delegate`).
3. Never define shared constants (such as preference, notification payload, or
   dictionary keys) in coordinator or mediator headers when model-layer classes
   consume them, and never import coordinator or mediator headers into
   model-layer components.
4. When data types, constants (e.g., `const char[]`,
   `constexpr std::string_view`, `NSString* const`), pref name constants,
   feature flags, configuration types, or utility functions must be shared
   across layers, relocate them to the model layer (e.g.,
   `<feature>/model/<feature>_constants.h`), a shared pref names file, or a
   designated `public/` directory (such as `<feature>/public/` or
   `shared/public/`).

**Why.** In Chromium iOS modular architecture, UI, coordinator, and model layers
are strictly decoupled to maintain clean architectural layering, enforce
dependency inversion, prevent circular dependencies, and minimize build times.
UI components must interact with models only through coordinators and mediators;
importing model headers into UI bypasses presentation abstraction and leaks
internal details. Conversely, the model layer sits below the coordinator and UI
layers; allowing model components to depend upwards on `ui/`, `ui_bundled/`, or
`coordinator/`—whether by adding DEPS exceptions to reuse UI delegate protocols,
or by importing coordinator/mediator headers to access shared
constants—introduces reverse architectural dependencies and breaks modular layer
encapsulation.

## Examples

Bad:

```cpp
// 1. UI depending on model header for callback typedef:
// ios/chrome/browser/feature/ui/feature_bottom_sheet_data_source.h:
#import "ios/chrome/browser/favicon/model/favicon_loader.h"

@protocol FeatureBottomSheetDataSource
- (void)loadFaviconForURL:(const GURL&)URL
                 callback:(FaviconLoader::FaviconAttributesCompletionBlock)callback;
@end

// 2. Model punching DEPS exception and importing UI header for delegate protocol:
// ios/chrome/browser/feature/model/DEPS:
include_rules = [
  "+ios/chrome/browser/feature/ui/feature_view_controller_delegate.h",
]
// ios/chrome/browser/feature/model/feature_tab_helper.h:
#import "ios/chrome/browser/feature/ui/feature_view_controller_delegate.h"
class FeatureTabHelper : public web::WebStateUserData<FeatureTabHelper> {
 public:
  void SetDelegate(id<FeatureViewControllerDelegate> delegate);
};

// 3. Model importing coordinator/mediator header for shared constants:
// ios/chrome/browser/feature/coordinator/feature_mediator.h:
extern const char kFeatureItemTimeKey[];
// ios/chrome/browser/feature/model/feature_client.mm:
#import "ios/chrome/browser/feature/coordinator/feature_mediator.h"
```

Good:

```cpp
// 1. Local callback alias and forward-declared types in UI, no model import:
// ios/chrome/browser/feature/ui/feature_bottom_sheet_data_source.h:
@class FaviconAttributes;

using FaviconAttributesCallback = void (^)(FaviconAttributes* attributes);

@protocol FeatureBottomSheetDataSource
// Loads the favicon for `URL` and invokes `callback` with it.
- (void)loadFaviconForURL:(const GURL&)URL
                 callback:(FaviconAttributesCallback)callback;
@end

// 2. Defining delegate protocol in consumer model layer:
// ios/chrome/browser/feature/model/feature_tab_helper_delegate.h:
class FeatureTabHelper;

@protocol FeatureTabHelperDelegate
- (void)featureTabHelper:(FeatureTabHelper*)tabHelper
    didDetectContentForURL:(const GURL&)URL;
@end
// ios/chrome/browser/feature/model/feature_tab_helper.h:
@protocol FeatureTabHelperDelegate;
class FeatureTabHelper : public web::WebStateUserData<FeatureTabHelper> {
 public:
  void SetDelegate(id<FeatureTabHelperDelegate> delegate);
};

// 3. Defining shared constants in model/ or public/ instead of coordinator:
// ios/chrome/browser/feature/model/feature_constants.h:
extern const char kFeatureItemTimeKey[];
// ios/chrome/browser/feature/model/feature_client.mm:
#import "ios/chrome/browser/feature/model/feature_constants.h"
```
