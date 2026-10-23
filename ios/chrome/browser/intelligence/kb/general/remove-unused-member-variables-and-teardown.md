---
name: ios/remove-unused-member-variables-and-teardown
description: When authoring or refactoring classes, view controllers, or coordinators, remove unused member variables, ivars, properties, obsolete initializer parameters, member initializers, and leftover teardown calls
rubric: readability
severity: should_fix
citations:
  - path: agents/projects/code-health/hub/references/automated_review.md
    heading: Shared Automated Review Protocol > Step by Step Instructions > Functionality & Verification (CRITICAL)
    quote: Ensure any unused methods, variables, or imports resulting from the change are also removed. Check callers, headers, and tests for completeness.
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 08. **Remove dead code**
    quote: In all removed lines, check for functions, classes, and constants that are no longer referenced and delete their declarations and definitions.
---

**Rule.** Do not retain unused member variables, ivars, or unbacked properties
in class, struct, or @implementation declarations, and remove unused constructor
or method parameters and member initializers. When authoring, refactoring, or
cleaning up classes, view controllers, coordinators, or mediators:

- Ensure every declared member is actively read, mutated, or inspected in the
  implementation; remove obsolete members and unreferenced initializers rather
  than leaving dead state.
- Audit members that bypass compiler -Wunused-private-field
  diagnostics—specifically types with non-trivial constructors or destructors
  such as base::WeakPtrFactory, callbacks, or smart pointers—and eliminate
  boilerplate instances (e.g., base::WeakPtrFactory) when no callbacks or weak
  pointers are generated.
- When an initialization dependency is only needed during construction or setup,
  pass it directly or consume it immediately rather than storing it as backing
  member state.
- When removing, refactoring, or bundling views, components, or parameters into
  configuration objects, remove obsolete parameters from method signatures and
  initializer lists, delete unused backing ivars/members, and clean up leftover
  lifecycle configuration, property assignments (which silently no-op on nil),
  and orphaned teardown or reset calls.

**Why.** Unused member variables, ivars, and constructor initializers bloat
object layout size, increase memory footprints, and clutter class definitions.
They mislead maintainers into expecting active lifetime tracking, callback
dispatch, view hierarchy ownership, or state observation. Furthermore: - Unused
members often signal incomplete feature implementations or dropped lifecycle
logic from rebases or merge conflicts. - Because types with non-trivial
constructors (such as base::WeakPtrFactory or smart pointers) suppress
-Wunused-private-field warnings, and Objective-C messages to nil ivars fail
silently without warnings or runtime errors, dead state and lingering lifecycle
updates easily escape compiler detection. - Retaining unused constructor
parameters and assigning them to dead backing fields masks dead code from
-Wunused-parameter diagnostics and clutters public APIs.

## Examples

Bad:

```cpp
// Unused ivar, dead parameter stored, and silent nil assignment in lifecycle:
@implementation FeatureViewController {
  UIImageView* _promoView;  // View removed, but ivar retained.
  int _index;               // Backing ivar never read downstream.
}
- (instancetype)initWithIndex:(int)index {
  if ((self = [super init])) { _index = index; }
  return self;
}
- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  _promoView.image = self.logo;  // Dead property assignment on nil.
}

// Dead member state and unneeded WeakPtrFactory/teardown in C++:
class DataExporter {
 public:
  DataExporter(ProfileIOS* profile) : profile_(profile), weak_factory_(this) {}
  void Disconnect() { timeout_closure_.Cancel(); }  // Never scheduled.
 private:
  raw_ptr<ProfileIOS> profile_;
  bool is_active_ = false;  // Never read or modified.
  base::WeakPtrFactory<DataExporter> weak_factory_;  // No callbacks generated.
};
```

Good:

```cpp
// Dead ivars, the unused initializer parameter, and the lifecycle update on
// the removed view are deleted outright; the class inherits -init and
// -viewDidLayoutSubviews instead of overriding them with no-ops:
@implementation FeatureViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor systemBackgroundColor];
}

@end

// Active members only; the unused factory, flag, and Disconnect() teardown
// are removed rather than left as empty shells:
class DataExporter {
 public:
  explicit DataExporter(ProfileIOS* profile) : profile_(profile) {}

 private:
  raw_ptr<ProfileIOS> profile_;
};
```
