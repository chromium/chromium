---
name: ios/consumer-protocols-pass-simple-value-types-not-model-objects
description: When declaring UI layer protocols (including consumer, mutator, and handler protocols), pass simple value types or Objective-C info wrappers instead of raw C++ model objects
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Data Types Passed Over Consumer Protocols
    quote: Consumer protocols must never pass model-layer objects or structs. Doing so leaks the model layer into the UI layer and couples the view controller to the model.
---

**Rule.** UI layer protocols (such as consumer, mutator, handler, and delegate
protocols) must never expose or pass raw C++ model-layer classes or structs
(such as `bookmarks::BookmarkNode*`, `syncer::SyncService*`, or
`const TabGroup*`). In consumer protocols, mediators must map model state to
simple Foundation types, enums, or primitives. In handler, mutator, and delegate
protocols, pass simple value types or lightweight Objective-C item/info wrapper
objects (such as `TabGroupInfo*` or `TabSwitcherItem*`) instead of introducing
`#ifdef __cplusplus` guards or C++ model headers into Objective-C protocols.

**Why.** In Chrome for iOS architecture, the UI layer is decoupled from backend
C++ data models. Passing raw C++ pointers or structs across UI protocols leaks
model implementation details into the UI layer, complicates unit testing by
requiring model fixtures or fakes, and forces `#ifdef __cplusplus` guards into
Objective-C protocol headers. See
`ios/architecture/mediators.md#data-types-passed-over-consumer-protocols`.

## Examples

Bad:

```cpp
// Bad: Consumer protocol passing a C++ model struct.
@protocol BookmarkConsumer <NSObject>
- (void)setBookmarkNode:(const bookmarks::BookmarkNode*)node;
@end

// Bad: UI handler protocol exposing a raw C++ model object.
#ifdef __cplusplus
class TabGroup;
#endif

@protocol TabCollectionDragDropHandler
#ifdef __cplusplus
- (BOOL)isGroupShared:(const TabGroup*)group;
#endif
@end
```

Good:

```cpp
// Good: Consumer protocol passing simple Foundation types and primitives.
@protocol BookmarkConsumer <NSObject>
- (void)setBookmarkTitle:(NSString*)title URL:(const GURL&)URL;
@end

// Good: UI handler protocol passing an Objective-C info wrapper object.
@class TabGroupInfo;

@protocol TabCollectionDragDropHandler
- (BOOL)isGroupShared:(TabGroupInfo*)group;
@end
```
