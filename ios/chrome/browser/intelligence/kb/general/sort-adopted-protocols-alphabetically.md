---
name: ios/sort-adopted-protocols-alphabetically
description: In Objective-C and Objective-C++ interface, category, and class extension declarations, sort adopted protocol conformance lists (`<...>`) alphabetically.
rubric: readability
severity: nit
---

**Rule.** In Objective-C and Objective-C++ class, category, and class extension
interface declarations (`@interface ... <...>`), keep conforming protocol lists
sorted in strict alphabetical order. When adding a new protocol conformance
(such as delegate, command, consumer, observer, handler, or presenter protocols)
to an existing interface, insert it at its lexicographical position rather than
appending it to the end of the list.

**Why.** Clang-format does not automatically sort protocol conformance lists in
Objective-C interface declarations. Maintaining alphabetical ordering keeps
protocol adoptions consistent, predictable, and easily discoverable as classes
adopt multiple delegates, observers, and commands. It also prevents duplicate
protocol conformances and reduces merge conflicts when concurrent changes extend
the same interface.

## Examples

Bad:

```cpp
// Appending new delegate or command protocols out of alphabetical order:
@interface FullscreenSigninScreenCoordinator () <
    IdentityChooserCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate,
    TOSCoordinatorDelegate,
    SceneStateObserver>

@interface BrowserCoordinator () <
    BookmarkInteractionControllerDelegate,
    WhatsNewCommands,
    DockingPromoCommands>
```

Good:

```cpp
// Inserting protocols in strict alphabetical order:
@interface FullscreenSigninScreenCoordinator () <
    IdentityChooserCoordinatorDelegate,
    SceneStateObserver,
    TOSCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>

@interface BrowserCoordinator () <
    BookmarkInteractionControllerDelegate,
    DockingPromoCommands,
    WhatsNewCommands>
```
