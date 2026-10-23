---
name: ios/avoid-raw-ptr-in-objective-c-properties
description: Never use raw_ptr<T>, raw_ref<T>, or base::WeakPtr<T> in Objective-C @property declarations or method parameters; use bare C++ pointers (T* or const T*) on the Objective-C interface instead
rubric: safety
severity: should_fix
citations:
  - path: styleguide/c++/c++.md
    heading: Non-owning pointers in class fields
    quote: Class and struct fields that are not garbage-collected should be written const raw_ref<T> or raw_ptr<T> rather than T& or T* whenever possible.
  - path: base/memory/raw_ptr.md
    heading: Pointers in locations other than fields
    quote: 'Use raw C++ pointers instead of raw_ptr<T> in the following scenarios: - Pointers in local variables and function parameters and return values.'
---

**Rule.** Never use `raw_ptr<T>`, `raw_ref<T>`, or `base::WeakPtr<T>` in
Objective-C `@property` declarations or Objective-C method parameters. Declare
properties and method parameters that reference C++ objects with bare C++
pointers (`T*` or `const T*`) instead (see
`ios/synthesize-protocol-properties-over-trivial-getters` for implementing
explicit accessors backed by a `raw_ptr<T>` or `base::WeakPtr<T>` ivar).

**Why.** `raw_ptr<T>`, `raw_ref<T>`, and `base::WeakPtr<T>` are intended for
persistent member variables and instance variables (`@implementation` ivars),
not public interfaces or transient parameter passing. Exposing C++ smart-pointer
or wrapper templates in `@property` declarations or Objective-C method
parameters leaks storage details into headers, breaks synthesized accessors and
protocol signatures, and adds unnecessary reference-counting overhead at call
sites.

## Examples

Bad:

```cpp
@interface FooMediator : NSObject
@property(nonatomic, assign) raw_ptr<const bookmarks::BookmarkNode> folder;
- (instancetype)initWithPrefService:(raw_ptr<PrefService>)prefService;
@end
```

Good:

```cpp
@interface FooMediator : NSObject
@property(nonatomic, assign) const bookmarks::BookmarkNode* folder;
- (instancetype)initWithPrefService:(PrefService*)prefService;
@end
```
