---
name: ios/reuse-local-variable-for-repeated-flag-or-predicate-checks
description: When repeatedly querying feature flags, service factories, properties, dictionary lookups, or chained accessors, cache in a local variable or helper
rubric: design
severity: should_fix
citations:
  - path: styleguide/c++/c++.md
    heading: Unnamed namespaces
    quote: Items local to a .cc file should be wrapped in an unnamed namespace.
  - path: docs/mac/arc.md
    heading: The basics of ARC
    quote: Objective-C objects are accessed via pointer. The most straightforward way of thinking about ARC is that, while in classic manual reference counting, those pointers are raw pointers and the programmer is in charge of writing the appropriate retain and release messages, with ARC, all pointers to Objective-C objects are smart pointers
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** Do not evaluate duplicate condition expressions or repeatedly query
the same feature flag (e.g., `Is<Feature>Enabled()`), getter (e.g.,
`GetWeakPtr()`), dictionary lookup (e.g., `dict->FindBool()`), Objective-C
property (`self.property`, `[self property]`, or `object.property`), keyed
service factory (e.g., `ProfileKeyedServiceFactoryIOS` helpers like
`*Factory::GetForProfile()`), or chained accessor / service-locator call (e.g.,
`self.profile->IsOffTheRecord()`, `shortcut_item.type`,
`self.browser->GetCommandDispatcher()`,
`GetApplicationContext()->GetLocalState()`,
`tabGrid.view.window.rootViewController`) within a function, method, or
initializer. Bind the evaluated result, pointer, reference, or dependency
instance to a descriptive local variable or const reference before first use,
and reuse it across subsequent statements, branches, assignments, guards,
collaborator instantiations, and argument expressions rather than repeating the
lookup inline. When an object or dependency (such as `self.profile`,
`self.browser`, or a delegate) has already been extracted into a local variable,
reuse that local variable for all subsequent member invocations, factory calls,
or handler registrations instead of re-accessing via `self` or repeating factory
lookups. Additionally, when checking a lookup or optional getter in a condition,
bind the result first and unwrap (`.value()`) or reuse that local variable in
the guarded body. When the same multi-step retrieval path recurs across several
methods, factor it into a file-local helper rather than duplicating the
traversal.

**Why.** Repeatedly querying feature flags, getters, dictionary keys, service
factories, or Objective-C properties incurs redundant traversals, redundant map
lookups in service factories, dynamic method dispatch (`objc_msgSend`), and
unnecessary ARC retain/release churn. In Objective-C and Objective-C++, dot
notation (`object.property`) and message expressions (`[object property]`)
execute dynamic dispatch on every call. Repeating service factory lookups inline
clutters collaborator instantiation, introduces redundant map queries, and risks
subtle inconsistencies if different profile accessors (such as a local parameter
versus a member accessor) are passed to different calls. Deeply chained
accessors, property traversals, and service-locator calls clutter expressions,
cause awkward line wrapping, add visual noise, and incur redundant lookup
overhead. Re-evaluating lookups or conditions inside guarded blocks risks state
desynchronization, while re-accessing `self.property` when an extracted variable
exists adds visual noise and invites inconsistent references. Binding the result
once to a local intermediate variable establishes a single source of truth,
eliminates redundant evaluations, clarifies data dependencies and flow, and
guarantees all collaborators receive the identical instance; a shared helper
does the same for a traversal repeated across methods.

## Examples

Bad:

```cpp
// Repeating factory lookup across collaborator instantiations:
_savedPasswordsPresenter = std::make_unique<SavedPasswordsPresenter>(
    options, IOSPasskeyModelFactory::GetForProfile(self.profile));
_mediator = [[PasswordSettingsMediator alloc]
    initWithModel:IOSPasskeyModelFactory::GetForProfile(profile)];

// Repeating chained property or lookup without caching:
if (!dict->FindBool(kKey)) return;
[tabGrid.view.window.rootViewController addChildViewController:layout];
layout.view.frame = tabGrid.view.window.rootViewController.view.bounds;

// Duplicating chained traversal across methods instead of a helper:
+ (BOOL)hasSeed { return GetAppContext()->GetVariations()->GetSeed()->Read(); }
+ (void)setSeed { GetAppContext()->GetVariations()->GetSeed()->Store(); }
```

Good:

```cpp
// Cache factory lookup once and reuse across collaborators:
PasskeyModel* passkeyModel = IOSPasskeyModelFactory::GetForProfile(profile);
_savedPasswordsPresenter =
    std::make_unique<SavedPasswordsPresenter>(options, passkeyModel);
_mediator = [[PasswordSettingsMediator alloc] initWithModel:passkeyModel];

// Cache lookup and intermediate property:
std::optional<bool> is_set = dict->FindBool(kKey);
if (!is_set) return;
UIViewController* rootViewController = tabGrid.view.window.rootViewController;
[rootViewController addChildViewController:layout];
layout.view.frame = rootViewController.view.bounds;

// Extract repeated chained traversal to an unnamed-namespace helper:
namespace {
SeedStore* GetSeed() { return GetAppContext()->GetVariations()->GetSeed(); }
}  // namespace
+ (BOOL)hasSeed { return GetSeed()->Read(); }
+ (void)setSeed { return GetSeed()->Store(); }
```
