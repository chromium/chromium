---
name: ios/import-header-over-casting-forward-declared-type
description: When assigning or passing Objective-C objects to protocol types, omit redundant casts and temporary variables; rely on implicit conversion, header imports, or compound conformances
rubric: readability
severity: should_fix
citations:
  - path: docs/ios/style.md
    heading: Use ObjCCast<T> and ObjCCastStrict<T>
    quote: As the C++ style guide tells you, we never use C casts and prefer `static_cast<T>` and `dynamic_cast<T>`.
---

**Rule.** Do not use explicit inline casts (such as `(id<Protocol>)`,
`static_cast<id<Protocol>>()`, `(id<NSObject>)`, or `(id)`) or introduce
single-use temporary variables when assigning or passing Objective-C objects to
protocol types or bridge constructors (such as `ObserverBridge`). If the
object's class already conforms to the expected protocol, pass or assign it
directly and rely on safe implicit conversion. If passing an instance of a
forward-declared class, `#import` its interface header (`.h`) so Clang verifies
protocol adoption rather than bypassing type checks with an explicit cast.
Additionally, if an object or property must satisfy multiple protocol contracts,
declare compound protocol conformances (`id<ProtocolA, ProtocolB>`) directly on
the property or parameter declaration instead of casting at call sites. The
exception is `*Commands` protocols, which must never be combined into one
`id<FooCommands, BarCommands>` property: declare one weak `<name>Handler`
property per command protocol instead.

**Why.** In Objective-C, assigning an object whose class adopts a protocol to an
`id<Protocol>` parameter, variable, or C++ bridge constructor is handled safely
via implicit conversion. Adding explicit C-style casts like `(id<Protocol>)`,
`static_cast<id<Protocol>>()`, or single-use intermediate variables introduces
visual clutter, indirection, and unnecessary boilerplate. Furthermore, explicit
casting bypasses Clang's static type verification, masking missing `#import`
headers for forward-declared classes or hiding subtle type mismatches when
protocol signatures evolve.

## Examples

Bad:

```cpp
// Redundant cast and intermediate temporary when passing to bridge:
id<FullscreenBrowserAgentObserving> observer =
    (id<FullscreenBrowserAgentObserving>)_consumer;
_observer = std::make_unique<FullscreenBrowserAgentObserverBridge>(
    observer, _browserAgent);

// Casting forward-declared type instead of importing header:
[self.consumer.traitOverrides
    setObject:(id<NSObject>)CreateColorPaletteFromSeedColor(...)
     forTrait:NewTabPageTrait.class];

// Casting property instead of declaring compound protocol conformance:
@property(nonatomic, weak) id<ComposeboxDebuggerLogger> debugLogger;
_tabPickerCoordinator.logger = (id<TabPickerLogger>)self.debugLogger;
```

Good:

```cpp
// Pass conforming object directly to bridge without cast or temporary:
_observer = std::make_unique<FullscreenBrowserAgentObserverBridge>(
    _consumer, _browserAgent);

// Import interface header so compiler verifies protocol adoption:
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
[self.consumer.traitOverrides
    setObject:CreateColorPaletteFromSeedColor(...)
     forTrait:NewTabPageTrait.class];

// Declare compound protocol conformance on property:
@property(nonatomic, weak) id<ComposeboxDebuggerLogger, TabPickerLogger>
    debugLogger;
_tabPickerCoordinator.logger = self.debugLogger;
```
