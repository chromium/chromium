---
name: ios/avoid-raw-ptr-in-objective-c-properties
rubric: safety
severity: should_fix
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
