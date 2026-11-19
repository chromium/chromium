---
name: ios/consumer-protocols-pass-simple-value-types-not-model-objects
description: When declaring UI layer protocols (including consumer, consumer data source, mutator, and view controller delegate protocols), pass simple value types instead of model-layer objects or structs.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Data Types Passed Over Consumer Protocols
    quote: Consumer protocols must never pass model-layer objects or structs. Doing so leaks the model layer into the UI layer and couples the view controller to the model.
---

**Rule.** UI layer protocols (such as consumer, consumer data source, mutator,
and view controller delegate protocols) must never expose or pass model-layer
objects or structs (such as `bookmarks::BookmarkNode*`, `syncer::SyncService*`,
`web::WebState*`, or `const TabGroup*`). Instead, convert model state into
simple value types suitable for the UI—typically Foundation types (such as
`NSString*`), primitives, `enum class` values, `GURL`, `UIImage*`, or
lightweight identifiers (such as `base::Uuid` or `web::WebStateID`).

**Why.** Passing model-layer objects or structs across UI protocols leaks the
model layer into the UI layer and couples view controllers to the model. Passing
simple value types keeps the layers decoupled. See
`ios/architecture/mediators.md#data-types-passed-over-consumer-protocols`.

## Examples

Bad:

```cpp
@protocol BookmarkConsumer <NSObject>
- (void)setBookmarkNode:(const bookmarks::BookmarkNode*)node;
@end
```

Good:

```cpp
@protocol BookmarkConsumer <NSObject>
- (void)setBookmarkTitle:(NSString*)title URL:(const GURL&)URL;
@end
```
