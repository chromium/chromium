---
name: ios/avoid-bare-id-in-objc
description: When declaring Objective-C variables or parameters, use concrete class pointers or protocol-qualified types instead of bare id, and pass validated downcast variables
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Type Deduction (`auto`), `id`, and Literals
    quote: Unlike `auto`, `id` disables static type checking. Reserve `id` for protocol-qualified references (`id<FooConsumer>`, `id<FooCommands>`). Always write the concrete class type when known.
---

**Rule.** Do not use bare, untyped `id` declarations for local variables,
properties, method parameters, or return types in Objective-C and Objective-C++
when a concrete type or protocol is known. Specify the explicit concrete class
pointer (such as `UIView*` or `NSLayoutXAxisAnchor*`) or protocol-qualified type
(such as `id<Protocol>` or `id<NSObject>` for notification observer tokens)
instead. Additionally, when an untyped `id` parameter or object is received and
downcast or validated into a strongly typed local variable (such as via
`base::apple::ObjCCast<T>` or `base::apple::ObjCCastStrict<T>`), pass that
validated, strongly typed local variable to subsequent method calls rather than
continuing to pass or reference the raw untyped `id` argument.

**Why.** A bare `id` is dynamically typed and disables compiler type checking
and selector verification, obscuring the expected interface, risking runtime
crashes from unrecognized selectors, and creating selector ambiguity across
unrelated classes. Explicit static typing (`Class*`) and protocol qualifications
(`id<Protocol>`) enforce compiler verification at interface boundaries and
document caller intent. Passing the validated, strongly typed local variable
from `base::apple::ObjCCast<T>` downstream preserves type safety and guarantees
that subsequent invocations operate on the verified type rather than bypassing
safety guarantees with raw dynamic references.

## Examples

Bad:

```cpp
// Dynamic or bare id bypasses type checks and obscures interface:
id _deviceOrientationObserver = nil;
id leadingTargetAnchor = self.locationLabel.leadingAnchor;
- (void)handleItem:(id)item;

// Passing raw untyped id parameter instead of validated typed local:
- (void)handleURL:(id)idURL forItem:(NSExtensionItem*)item {
  NSURL* URL = base::apple::ObjCCast<NSURL>(idURL);
  if (!URL) return;
  [self handleSharedImage:idURL forItem:item];
}
```

Good:

```cpp
// Protocol-qualified and explicit concrete types preserve type safety:
id<NSObject> _deviceOrientationObserver = nil;
NSLayoutXAxisAnchor* leadingTargetAnchor = self.locationLabel.leadingAnchor;
- (void)handleItem:(id<ItemProtocol>)item;

// Pass validated strongly typed local variable to subsequent calls:
- (void)handleURL:(id)idURL forItem:(NSExtensionItem*)item {
  NSURL* URL = base::apple::ObjCCast<NSURL>(idURL);
  if (!URL) return;
  [self handleSharedImage:URL forItem:item];
}
```
