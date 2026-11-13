---
name: ios/mediators-store-owned-values-not-pointers-into-model-caches
description: In iOS mediators and coordinators, store owned copies, stable identifiers, or base::WeakPtr instead of raw pointers into model-owned caches or collections that can change while UI flows are pending
rubric: safety
severity: blocking
citations:
  - path: ios/best_practices.md
    heading: Managing C++ Lifetimes and Pointers
    quote: For unowned references, use `const raw_ref<T>` (from `base/memory/raw_ref.h`) when the reference can never be null, `raw_ptr<T>` (from `base/memory/raw_ptr.h`) when it may be null or reassigned, and `base::WeakPtr<T>` when the pointee may be destroyed asynchronously.
---

**Rule.** In Chrome iOS mediators and coordinators, do not keep raw pointers
(`T*`, `const T*`, `raw_ptr<T>`, or containers such as `std::vector<const T*>`)
to objects owned by a model-layer cache, model, or collection (for example
entries returned by a form manager, a list of suggestions, or navigation items)
across asynchronous UI flows such as presentation, user input, or animations.
Store owned copies (`std::vector<T>`), stable identifiers (GUIDs,
`web::WebStateID`), or `base::WeakPtr<T>`, and resolve the object again when the
user acts.

**Why.** Mediators often outlive the backend event that produced their data:
navigations, profile changes, or cache refreshes can free or reallocate the
pointed-to entries while a sheet or menu is still on screen. Raw pointers into
those caches then dangle, and dereferencing them when the user taps an item is a
use-after-free. See `ios/best_practices.md#managing-c-lifetimes-and-pointers`.

## Examples

Bad:

```cpp
@implementation FooSuggestionsMediator {
  // BAD: Points into `FooModel`'s cache, which is rebuilt on navigation.
  std::vector<const FooEntry*> _entries;
}

- (void)didSelectEntryAtIndex:(NSUInteger)index {
  _fooModel->Apply(*_entries[index]);  // BAD: May be dangling.
}
@end
```

Good:

```cpp
@implementation FooSuggestionsMediator {
  // Owned copies stay valid however long the UI is shown.
  std::vector<FooEntry> _entries;
}

- (void)didSelectEntryAtIndex:(NSUInteger)index {
  _fooModel->ApplyEntryWithGUID(_entries[index].guid());
}
@end
```
