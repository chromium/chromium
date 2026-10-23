---
name: ios/extract-duplicate-logic-into-shared-helper
description: When methods, sibling classes, view controllers, mediators, or tests duplicate logic, formatting, or enums, extract shared helpers into utility files or components
rubric: design
severity: should_fix
---

**Rule.** Extract duplicated logic, presentation formatting, branching,
dispatch, multi-step configuration, geometry or offset calculations, or enum
types into a shared helper method, dedicated feature utility file (such as
`*_util.h`/`.mm`), common utility class, or extracted component instead of
repeating identical multi-line statements, switch statements, condition checks,
formatting routines, view or `UIBarButtonItem` setup, transform/alpha/inset
calculations, or enums across methods, lifecycle hooks, sibling callbacks,
file-local anonymous namespaces, sibling view controllers, mediators,
coordinators, or test cases. Keep shared state mutation, property configuration,
validation, branching, and notification logic centralized in a single
authoritative source of truth, and when sibling methods update related UI state
(animation or scroll progress, presentation modes, view transforms, alpha,
layout insets), compose them directly by delegating the shared update to the
more granular method rather than duplicating it. Keep callers, entry points, and
observer methods focused on high-level orchestration, condition gating, and
control flow. When sharing logic across collaborating classes or during
incremental component migrations, parameterize standalone helpers by accessible
dependencies (such as scene states, command handlers, or test parameters) or
delegate from call sites to the shared utility or extracted component; in
`//ios/chrome`, utility functions live in the global namespace.

**Why.** Duplicating multi-line configuration, formatting routines, type-mapping
switch statements, validation checks, affine transforms, safe-area insets,
opacity or geometry calculations, enums, or notification logic across sibling
methods, callbacks, anonymous namespaces, collaborating classes, view
controllers, mediators, or test cases creates maintenance hazards, binary bloat,
and behavioral drift when one copy is updated and another is overlooked, and
bloats handlers so the high-level coordination flow is obscured. Centralizing
shared routines and types into an authoritative helper method, feature utility
file, utility class, or extracted component, and composing sibling methods
instead of repeating their bodies, enforces DRY principles, guarantees
consistent behavior and error handling, simplifies updates in a single place,
enables targeted hermetic unit testing, and keeps callers concise and
declarative.

## Examples

Bad:

```cpp
// Bad: Duplicating validation dispatch across sibling methods.
- (void)tableViewItemDidChange:(TableViewTextEditItem*)item {
  if (item.type == ItemTypeCard) { isValid = [self validateCard]; }
}
- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)item {
  if (item.type == ItemTypeCard) { isValid = [self validateCard]; }
}

// Bad: Duplicating helpers and enums in anonymous namespaces across mediators.
namespace {
enum class EngineState { kGoogle, kNotGoogle };
EngineState GetState(TemplateURLService* s) {
  return s->GetDefaultSearchProvider() ? EngineState::kGoogle : EngineState::kNotGoogle;
}
}  // namespace

// Bad: Duplicating UI element configuration across methods:
- (void)viewDidLoad {
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(dismiss)];
}
- (void)updateNavigationBar {
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(exitEditMode)];
}

// Bad: Duplicating transform and alpha calculations across progress methods:
- (void)setNTPScrollProgressForToolbar:(CGFloat)progress {
  [self updateToolbarVisibility];
  _container.transform = CGAffineTransformMakeTranslation(0.0, kPadding * (1.0 - progress));
  self.view.alpha = progress;
}
- (void)setNTPScrollProgressForOmnibox:(CGFloat)progress {
  _container.transform = CGAffineTransformMakeTranslation(0.0, kPadding * (1.0 - progress));
  _container.alpha = progress;
}
```

Good:

```cpp
// Good: Extract shared validation helper or delegate to extracted component.
- (BOOL)isItemValid:(TableViewTextEditItem*)item {
  return item.type == ItemTypeCard ? [self validateCard] : YES;
}

// Good: Centralize shared enums and helpers in a feature utility header
// (foo_util.h), as global-namespace free functions.
_state = GetDefaultSearchEngineState(_templateURLService);

// Good: Factor UI element configuration into a dedicated helper method:
- (UIBarButtonItem*)createDoneButtonWithAction:(SEL)action {
  return [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                       target:self
                                                       action:action];
}

// Good: Compose sibling methods so the shared update lives in one place:
- (void)setNTPScrollProgressForToolbar:(CGFloat)progress {
  [self updateToolbarVisibility];
  [self setNTPScrollProgressForOmnibox:progress];
  self.view.alpha = progress;
}
- (void)setNTPScrollProgressForOmnibox:(CGFloat)progress {
  _container.transform = CGAffineTransformMakeTranslation(0.0, kPadding * (1.0 - progress));
  _container.alpha = progress;
}
```
