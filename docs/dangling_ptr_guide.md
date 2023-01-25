# Dangling Pointer Guide

A dangling pointer has been found in your patch? This doc will help you fix it.

> This document can also be commented upon:
https://docs.google.com/document/d/19yyWdCQB5KwYtRE5cwadNf30jNSSWwMbim1KJjc-eZQ/edit#

See also the general instructions about the dangling pointer detector:
[docs/dangling_ptr.md](./dangling_ptr.md)

**Table of content**
- [`Case 1` I don’t own the affected component](#i-don_t-own-the-affected-component)
- [`Case 2` The dangling pointer does not own the deleted object.](#the-dangling-pointer-does-not-own-the-deleted-object)
  - [Incorrect destruction order](#incorrect-destruction-order)
  - [Observer callback](#observer-callback)
  - [Challenging lifespan](#challenging-lifespan)
  - [Fallback solution](#fallback-solution)
- [`Case 3` The pointer manages ownership over the object](#the-pointer-manages-ownership-over-the-object)
  - [Smart pointers](#smart-pointers)
  - [Object vended from C API](#object-vended-from-c-api)
  - [Object conditionally owned](#object-conditionally-owned)
  - [Fallback solution](#fallback-solution-1)

## `Case 1` I don’t own the affected component

If you do not directly own the affected component, you **don't need** to solve
the issue yourself… though doing so is a great way to learn about and improve
the codebase.

Please annotate the pointer with `DanglingUntriaged`, and indicate the test case
that can be used to reproduce.
```cpp
// Dangling when executing TestSuite.TestCase on Windows:
raw_ptr<T, DanglingUntriaged> ptr_;
```
Opening and filling a P2 bug is a nice thing to do, but it is not required.

**Rationale:**
Engineers might uncover new dangling pointers, by testing new code paths.
Knowing about dangling pointers is a purely positive increment. In some cases,
the affected component belongs to a different team. We don’t want to disrupt
engineers achieving their primary goal, if they only “discover” a dangling
pointer. Annotating the pointer makes the issue visible directly in the code,
improving our knowledge of Chrome.

## `Case 2` The dangling pointer does not own the deleted object

### Incorrect destruction order

This represents ~25% of the dangling pointers.

In the majority of cases, this happens when dependent objects are declared in
the wrong order in a class, causing the dependency to be released first, thus
creating a dangling pointer in the other.

It is important to reorder them correctly to prevent pre-existing and future UAF
in destructors.

See [Fix member declaration order](https://docs.google.com/document/d/11YYsyPF9rQv_QFf982Khie3YuNPXV0NdhzJPojpZfco/edit?resourcekey=0-h1dr1uDzZGU7YWHth5TRAQ#bookmark=id.jgjtzldk9pvc) and [Fix reset ordering](https://docs.google.com/document/d/11YYsyPF9rQv_QFf982Khie3YuNPXV0NdhzJPojpZfco/edit?resourcekey=0-h1dr1uDzZGU7YWHth5TRAQ#bookmark=id.xdam727ioy4q) examples.

### Observer callback

This represents ~4% of the dangling pointers.

It is important to clear the pointer when the object is about to be deleted.
Chrome uses the observer pattern heavily. In some cases, the observer does not
clear its pointer toward the observed class when notified of its destruction.

### Challenging lifespan

It can be challenging to deal with an object's lifespan. Sometimes, the lifetime
of two objects are completely different.

Removing the pointer may be a good thing to do. Sometimes, it can be replaced
by:
-   Passing the pointer as a function argument instead of getting access to it
    from a long-lived field.
-   A token / ID. For instance
    [blink::LocalFrameToken](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/tokens/tokens.h;drc=898134d0d40dbbcd308e7d51655518ac7c6392b5;l=34),
    [content::GlobalRenderFrameHostId](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/global_routing_id.h;drc=898134d0d40dbbcd308e7d51655518ac7c6392b5;l=64)
-   A [WeakPtr](https://docs.google.com/document/d/11YYsyPF9rQv_QFf982Khie3YuNPXV0NdhzJPojpZfco/edit?resourcekey=0-h1dr1uDzZGU7YWHth5TRAQ#bookmark=id.geuhahom0twd)
-   [Calling a function](https://docs.google.com/document/d/11YYsyPF9rQv_QFf982Khie3YuNPXV0NdhzJPojpZfco/edit?resourcekey=0-h1dr1uDzZGU7YWHth5TRAQ#heading=h.wh99ri7bbq23)

### Fallback solution

As a last resort, when the situation is perfectly understood, and you believe it
is better to let the pointer dangle, the raw_ptr can be annotated with
`DisableDanglingPtrDetection`. A comment explaining why this is safe must be
added.

```cpp
// DisableDanglingPtrDetection option for raw_ptr annotates
// "intentional-and-safe" dangling pointers. It is meant to be used at the
// margin, only if there is no better way to re-architecture the code.
//
// Usage:
// raw_ptr<T, DisableDanglingPtrDetection> dangling_ptr;
//
// When using it, please provide a justification about what guarantees that it
// will never be dereferenced after becoming dangling.
using DisableDanglingPtrDetection =
    base::raw_ptr_traits::TraitBundle<base::raw_ptr_traits::MayDangle>;
```

**In emergency situations**: `DanglingUntriaged` can be used similarly, in case
your patch needs to land as soon as possible.

## `Case 3` The pointer manages ownership over the object

raw_ptr, just like raw pointers T*, are not meant to keep an object alive. It is
preferable not to manage memory manually using them and new/delete. Calling
delete on a raw_ptr will cause the raw_ptr to become immediately dangling.

### Smart pointers

First, consider replacing the raw_ptr with a smart pointer:

-   std::unique_ptr (See
    [example](https://docs.google.com/document/d/11YYsyPF9rQv_QFf982Khie3YuNPXV0NdhzJPojpZfco/edit?resourcekey=0-h1dr1uDzZGU7YWHth5TRAQ#heading=h.6itq8twigqt3))
-   scoped_refptr

### Object vended from C API

In some cases, the object is vended by a C API. It means new/delete are not
used, but some equivalent API. In this case, it still makes sense to use a
unique_ptr, but with a custom deleter:
[example](https://chromium-review.googlesource.com/c/chromium/src/+/3650764).

For most common objects, a wrapper already exists:
[base::ScopedFILE](https://source.chromium.org/chromium/chromium/src/+/main:base/files/scoped_file.h;drc=898134d0d40dbbcd308e7d51655518ac7c6392b5;l=105),
[base::ScopedTempDir](https://source.chromium.org/chromium/chromium/src/+/main:base/files/scoped_temp_dir.h;l=25?q=ScopedTempDir&sq=&ss=chromium%2Fchromium%2Fsrc),
..

### Object conditionally owned

In some cases, the raw_ptr conditionally owns the memory, depending on some
logic or some `is_owned` boolean. This can still use a unique_ptr
([example](https://chromium-review.googlesource.com/c/chromium/src/+/3829302))

```cpp
std::unique_ptr<T> t_owned_;
raw_ptr<T> t_; // Reference `t_owned_` or an external object.
```

### Fallback solution

If no solution with a smart pointer is found:

You can use `raw_ptr::ClearAndDelete()` or `raw_ptr::ClearAndDeleteArray()` to
clear the pointer and free the object in a single operation.

|Before|After |
|--|--|
| `delete ptr_` | `ptr_.ClearAndDelete();`|
| `delete array_` | `ptr_.ClearAndDeleteArray();`|

When delete is not used, but the deletion happens through some C API or some
external manager, the `raw_ptr::ExtractAsDangling()` can be used to clear the
pointer, and return the pointer to be passed to the API. The return value must
not be saved, thus outliving the line where it was called.

This method should be used wisely, and only if there is no other way to rework
the dangling raw_ptr.

|Before|After |
|--|--|
|`ExternalAPIDelete(ptr_);`|`ExternalAPIDelete(ptr_.ExtractAsDangling());`|
