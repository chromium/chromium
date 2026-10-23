---
name: ios/bind-objc-objects-weakly-in-base-bind
description: In Objective-C++, bind captureless C++ lambdas or free functions with base::Bind instead of blocks; bind weak references explicitly and move move-only types directly
rubric: safety
severity: should_fix
citations:
  - path: styleguide/c++/c++-features.md
    heading: std::shared_ptr
    quote: Unlike `base::RefCounted`, uses extrinsic rather than intrinsic reference counting. Could plausibly be used in Chromium, but would require significant migration.
  - path: docs/mac/arc.md
    heading: Blocks
    quote: Blocks retain all objects referenced in them. This example is of a block used in an Objective-C method that uses the “weak self” idiom to avoid a retain cycle.
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: Within an Objective-C++ source file, follow the style for the language of the function or method you're implementing.
---

**Rule.** In Objective-C++ (`.mm`) code, when writing a new callback for
`base::BindOnce` or `base::BindRepeating`, prefer captureless C++ lambdas
(`[](...) { ... }`) or C++ free functions/static helpers with explicitly bound
arguments over Objective-C blocks (`^(...) { ... }`) that capture state.
Wrapping a block with `base::BindOnce(block)` / `base::BindRepeating(block)` is
the documented block-to-callback conversion and is fine: use it when you already
hold a block that must be passed to a callback-taking API, or when the block is
a single method call on `weakSelf` (`base::BindOnce(^{ [weakSelf didFinish]; })`
with `__weak __typeof(self) weakSelf = self;` captured outside). Reserve
Objective-C blocks otherwise for Objective-C APIs that require a block. When
binding callbacks, follow these binding conventions:

1. **Objective-C objects**: When binding an Objective-C object weakly (such as
   `self`, `WKWebView`, UI components, or enclosing controllers), pass an
   explicit `__weak` variable (such as `weakSelf` or `weakWebView`) as the bound
   argument instead of a strong reference, and declare the lambda/function
   parameter as a strong reference (such as `__typeof(self) strong_self` or
   typed pointer) rather than `__weak`. Do not pass `self` with a `__weak`
   parameter, as `base::Bind` deduces storage from call-site arguments and will
   strongly retain `self` in `BindState`.
2. **Move-only types**: When binding callbacks that hold move-only RAII types
   (such as `base::ScopedClosureRunner` or `std::unique_ptr`), pass the
   move-only object directly as a bound argument using `std::move(...)` to a C++
   lambda or free function with `base::BindOnce`. Never wrap move-only types in
   `std::shared_ptr` to capture them inside an Objective-C block.

**Why.** Objective-C blocks implicitly capture referenced variables from their
enclosing lexical scope with strong ARC references. When passed across
asynchronous boundaries or stored in callbacks, implicit capture can
inadvertently retain views, view controllers, or delegates beyond their intended
lifecycles, causing retain cycles, memory leaks, or execution of callbacks on
detached objects. Binding a C++ lambda or free function forces all captured
dependencies to be declared explicitly at the `base::Bind` call site.
Furthermore, Objective-C blocks cannot capture move-only types by value, leading
authors to wrap move-only types in `std::shared_ptr`, which is banned in
Chromium style because it introduces unmanaged extrinsic shared ownership and
refcounting overhead. `base::BindOnce` natively supports move-only bound
arguments when paired with C++ callables, transferring ownership cleanly without
shared-pointer workarounds. Finally, under ARC, `base::Bind` deduces bound
storage types from call-site arguments rather than callable parameter types, so
passing `self` stores a strong reference in `BindState` regardless of the
parameter type. Binding an explicit `__weak` reference ensures the object is not
retained by the callback, while accepting a strong reference parameter allows
ARC to safely retain it only for the duration of the callback execution. A block
that only calls a method on a `weakSelf` captured outside it has none of these
hazards, which is why wrapping such a block is the sanctioned conversion.

## Examples

Bad:

```cpp
// Bad 1: Wrapping move-only RAII type in std::shared_ptr to capture in an Obj-C block.
auto clear_task = std::make_shared<base::ScopedClosureRunner>(std::move(closure));
manager->FetchCard(&card, base::BindOnce(^(const Card& c) {
  auto task = clear_task;
  [weakSelf handleCardFetched:c];
}));

// Bad 2: Passing self stores a strong reference in BindState despite __weak parameter.
base::OnceClosure closure = base::BindOnce(
    [](__weak __typeof(self) weak_self) { [weak_self handleCompletion]; }, self);

// Bad 3: Passing an inline block implicitly captures webView strongly under ARC.
self.webStateImpl->OnAuthRequired(
    space, base::BindRepeating(^(SecIdentityRef identity) {
      [CRWWKNavigationHandler processClientCert:identity webView:webView];
    }));
```

Good:

```cpp
// Good 1: Pass move-only type directly to base::BindOnce with a C++ lambda.
manager->FetchCard(
    &card, base::BindOnce(
               [](base::ScopedClosureRunner runner, __typeof(self) strong_self,
                  const Card& c) { [strong_self handleCardFetched:c]; },
               std::move(closure), weakSelf));

// Good 2: Bind explicit weakSelf and accept strong reference in lambda parameter.
__weak __typeof(self) weakSelf = self;
base::OnceClosure closure = base::BindOnce(
    [](__typeof(self) strong_self) { [strong_self handleCompletion]; }, weakSelf);

// Good 3: Bind a C++ free function with explicit __weak parameter.
__weak WKWebView* weakWebView = webView;
self.webStateImpl->OnAuthRequired(
    space, base::BindRepeating(&ProcessClientCert, weakWebView));

// Good 4: Wrapping a single-call block on weakSelf is the documented conversion.
__weak __typeof(self) weakSelf = self;
service->Start(base::BindOnce(^{
  [weakSelf serviceDidStart];
}));
```
