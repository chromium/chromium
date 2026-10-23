---
name: ios/avoid-unused-methods-in-public-headers
description: When declaring or refactoring C++ or Objective-C header interfaces, keep implementation helpers private, omit unused or speculative @property, method, or category declarations, and bundle definitions
rubric: readability
severity: should_fix
citations:
  - path: docs/cl_tips.md
    heading: Encapsulate complexity, but don’t over-abstract
    quote: Allowing for extensibility when it’s not necessary, creating abstractions where something concrete would suffice, or reaching for a design pattern when something simpler would work equally well adds unnecessary complexity to the codebase. The codebase is inherently mutable and additional abstractions can be added _if and when_ they are needed.
  - path: styleguide/objective-c/objective-c.md
    heading: Delineate interface implementations with `#pragma mark -`
    quote: This kind of grouping should happen for the implementations's public methods, superclass methods, private methods, and each protocol the implementation conforms to.
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 08. **Remove dead code**
    quote: In all removed lines, check for functions, classes, and constants that are no longer referenced and delete their declarations and definitions. You may need to delete entire source files.
---

**Rule.** Do not declare or retain speculative, unused, or internal helper
functions, methods, convenience initializers, or @property members in public C++
or Objective-C header interfaces unless active external callers require them.
When refactoring migrates callers internally or removes external call sites,
remove the declaration from the public header and keep the helper private to the
implementation file (in an anonymous namespace, class extension, or under a
`#pragma mark - Private` section). Additionally, provide corresponding
definitions in the implementation file (.cc, .mm, or .m) within the same change
rather than staging declarations prematurely, and delete dead declarations that
lack active callers or definitions. When deprecating feature variants, updating
architectures, or cleaning up abandoned prototypes, audit companion headers and
source files to cleanly delete uncalled properties (such as unused command
handlers, delegates, or view state), unreferenced category interfaces
(@interface Class (Category)), and residual target-action bindings or styling
logic.

**Why.** Exposing implementation-only helpers, speculative methods, or uncalled
@property declarations in public headers or class extensions unnecessarily
bloats the public API surface, leaks internal details, and misleads consumers
and reviewers about active dependencies. Bundling declarations with their
definitions and exposing interface symbols only when external callers require
them keeps interfaces minimal, ensures APIs are verified in context, and
prevents dead code that risks linker errors or breaks declaration-definition
pairing. Furthermore, retaining orphaned prototype declarations, unreferenced
category interfaces, or residual target-action registrations clutters
compilation units, adds maintenance overhead, and obscures active UI control
paths.

## Examples

Bad:

```cpp
// Variant 1: Unused speculative @property in header:
@interface PasswordCheckupCoordinator : ChromeCoordinator
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;  // Unused
@end

// Variant 2: Speculative C++ interface method added before callers are wired up:
class SystemIdentityManager {
 public:
  virtual bool DisplayMDMNotification(
      id<SystemIdentity> identity,
      NSArray<id<SystemIdentity>>* active_identities,
      HandleMDMCallback callback) = 0;
};

// Variant 3: Internal helper method exposed in public header instead of private .mm:
@interface SceneCoordinator : NSObject
- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion;
@end

// Variant 4: Lingering category and residual target-action binding:
@interface BaseGridMediator (ClosingAllItems)
@property(nonatomic, assign) BOOL closingAllItems;
@end
[self.fooButton addTarget:self action:@selector(openFoo) forControlEvents:UIControlEventTouchUpInside];
```

Good:

```cpp
// Variant 1: Keep public interface minimal; omit uncalled properties:
@interface PasswordCheckupCoordinator : ChromeCoordinator
@property(nonatomic, weak) id<PasswordCheckupCoordinatorDelegate> delegate;
@end

// Variant 2: Defer interface method until callers and definition land:
class SystemIdentityManager {
 public:
  // Method omitted; added in the follow-up CL implementing its callers.
};

// Variant 3: Keep internal helper method private to implementation file (.mm):
@implementation SceneCoordinator
#pragma mark - Private
- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  [_tabGridCoordinator stopChildCoordinatorsWithCompletion:completion];
}
@end

// Variant 4: Cleanly delete unreferenced categories and residual target actions:
// (Omit BaseGridMediator (ClosingAllItems) category and obsolete button target)
```
