---
name: ios/name-parameterized-block-types-with-typedef
rubric: readability
severity: should_fix
---

**Rule.** When declaring Objective-C and Objective-C++ block callbacks with
parameters in methods, protocols, or functions, define a named type alias
(`using` in Objective-C++, or `typedef` in pure Objective-C) with descriptive
argument names rather than inlining anonymous block signatures. For complex or
nested block callbacks (such as blocks that accept another callback block),
define separate named aliases for the intermediate and inner block types instead
of writing nested inline signatures. For standard parameterless or condition
callbacks, use standard typedefs from `base/ios/block_types.h` instead of
spelling out raw block signatures or declaring one-off custom aliases:

- Use `ProceduralBlock` instead of raw `void (^)(void)` or `void (^)()`
  signatures and custom typedefs.
- Use `ConditionBlock` instead of raw `bool (^)(void)` signatures.
- When converting closures via `base::CallbackToBlock`, explicitly declare the
  resulting no-argument block variable as `ProceduralBlock` rather than deducing
  it with `auto`.

**Why.** Inline anonymous and nested block signatures obscure argument
semantics, hinder readability, and duplicate parameter lists across interfaces
and mock implementations. Defining dedicated type aliases with named arguments
documents callback contracts and makes signatures reusable. Standardizing on
`base/ios/block_types.h` typedefs (`ProceduralBlock` and `ConditionBlock`)
eliminates redundant boilerplate across iOS APIs and avoids type ambiguity
introduced by deduced `auto` on conversion helpers like `base::CallbackToBlock`.

## Examples

Bad:

```cpp
#import "base/ios/block_types.h"

// Bad: raw signature or custom typedef for standard void/condition blocks:
- (void)cancelDownload:(void (^)())completion;
typedef void (^ShareStepCompletion)();
@property(nonatomic, copy) bool (^isReady)(void);

// Bad: auto hides block signature when converting base::OnceClosure:
auto block = base::CallbackToBlock(std::move(cancelCallback));

// Bad: anonymous inline parameterized block signature:
- (void)fetchImage:(NSString*)path
        completion:(void (^)(UIImage* image, NSError* error))completion;

// Bad: nested inline block signature without dedicated alias:
- (void)confirmChange:(void (^)(void (^)(BOOL canProceed)))confirmationBlock;
```

Good:

```cpp
#import "base/ios/block_types.h"

// Good: standard ProceduralBlock and ConditionBlock from block_types.h:
- (void)cancelDownload:(ProceduralBlock)completion;
@property(nonatomic, copy) ConditionBlock isReady;

// Good: explicit ProceduralBlock on CallbackToBlock:
ProceduralBlock block = base::CallbackToBlock(std::move(cancelCallback));

// Good: named type alias with descriptive parameter names:
using ImageCompletion = void (^)(UIImage* image, NSError* error);
- (void)fetchImage:(NSString*)path completion:(ImageCompletion)completion;

// Good: dedicated aliases for nested callbacks:
using ChangeCallback = void (^)(BOOL canProceed);
using ConfirmationBlock = void (^)(ChangeCallback callback);
- (void)confirmChange:(ConfirmationBlock)confirmationBlock;
```
