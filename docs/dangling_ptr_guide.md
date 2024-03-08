# Dangling Pointer Guide

A dangling pointer has been found in your patch? This doc will help you fix it.

> This document can also be commented upon:
https://docs.google.com/document/d/19yyWdCQB5KwYtRE5cwadNf30jNSSWwMbim1KJjc-eZQ/edit#

See also the general instructions about the dangling pointer detector:
[docs/dangling_ptr.md](./dangling_ptr.md)

**Table of contents**
- [What to do about dangling pointers](#what-to-do-about-dangling-pointers)
  - [`Case 1` I don’t own the affected component](#i-don_t-own-the-affected-component)
  - [`Case 2` The dangling pointer does not own the deleted object.](#the-dangling-pointer-does-not-own-the-deleted-object)
    - [Incorrect destruction order](#incorrect-destruction-order)
    - [Observer callback](#observer-callback)
    - [Challenging lifespan](#challenging-lifespan)
    - [Cyclic pointers](#cyclic-pointers)
    - [Fallback solution](#fallback-solution)
  - [`Case 3` The pointer manages ownership over the object](#the-pointer-manages-ownership-over-the-object)
    - [Smart pointers](#smart-pointers)
    - [Object vended from C API](#object-vended-from-c-api)
    - [Object conditionally owned](#object-conditionally-owned)
    - [Fallback solution](#fallback-solution-1)
- [What to do about unretained dangling pointers](./unretained_dangling_ptr_guide.md)
- [I can't figure out which pointer is dangling](I-can_t-figure-out-which-pointer-is-dangling)
- [FAQ - Why dangling pointers matter](#faq-why-dangling-pointers-matter)

## What to do about dangling pointers

There are a few common cases here.

### `Case 1` I don’t own the affected component

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

### `Case 2` The dangling pointer does not own the deleted object

#### Incorrect destruction order

This represents ~25% of the dangling pointers.

In the majority of cases, this happens when dependent objects are destroyed in
the wrong order in a class, causing the dependency to be released first, thus
creating a dangling pointer in the other.

Recall that destructors destroy class members in the inverse order of their
appearance. It is usually possible to resolve destruction order issues by
re-ordering member declarations so that members which need to live longer come
first. It is important to order members correctly to prevent pre-existing and
future UAF in destructors.

See [Fix member declaration order](https://docs.google.com/document/d/11YYsyPF9rQv_QFf982Khie3YuNPXV0NdhzJPojpZfco/edit?resourcekey=0-h1dr1uDzZGU7YWHth5TRAQ#bookmark=id.jgjtzldk9pvc) and [Fix reset ordering](https://docs.google.com/document/d/11YYsyPF9rQv_QFf982Khie3YuNPXV0NdhzJPojpZfco/edit?resourcekey=0-h1dr1uDzZGU7YWHth5TRAQ#bookmark=id.xdam727ioy4q) examples.

One good practice is make owning members (`unique_ptr<>`, `scoped_refptr<>`)
appear before unowned members (`raw_ptr<>`), and to make the unowned members
appear last in the class, since the unowned members often refer to resources
owned by the owning members or the class itself.

One sub-category of destruction order issues is related to `KeyedService`s which
need to correctly
[declare their dependencies](https://source.chromium.org/chromium/chromium/src/+/main:components/keyed_service/core/keyed_service_base_factory.h;l=60-62;drc=8ba1bad80dc22235693a0dd41fe55c0fd2dbdabd)
and
[are expected to drop references](https://source.chromium.org/chromium/chromium/src/+/main:components/keyed_service/core/keyed_service.h;l=12-13;drc=8ba1bad80dc22235693a0dd41fe55c0fd2dbdabd)
to their dependencies in their
[`Shutdown`](https://source.chromium.org/chromium/chromium/src/+/main:components/keyed_service/core/keyed_service.h;l=36-39;drc=8ba1bad80dc22235693a0dd41fe55c0fd2dbdabd) method
(i.e. before their destructor runs).

#### Observer callback

This represents ~4% of the dangling pointers.

It is important to clear the pointer when the object is about to be deleted.
Chrome uses the observer pattern heavily. In some cases, the observer does not
clear its pointer toward the observed class when notified of its destruction.

#### Cyclic pointers

Sometimes two (or more) objects can have unowned references between each other,
with neither one owning the other. This creates a situation where neither can
be deleted without creating a dangling pointer unless some action is first
taken to break the cycle. In order to create such a cycle in the first place, a
call to a "setter" method or equivalent must have occurred handing one object
a reference to the other. Balance out this call with another call to the same
setter, but passing nullptr instead, before the destroying the other object.

#### Challenging lifespan

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

#### Fallback solution

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
```

**In emergency situations**: `DanglingUntriaged` can be used similarly, in case
your patch needs to land as soon as possible.

### `Case 3` The pointer manages ownership over the object

raw_ptr, just like raw pointers T*, are not meant to keep an object alive. It is
preferable not to manage memory manually using them and new/delete. Calling
delete on a raw_ptr will cause the raw_ptr to become immediately dangling.

#### Smart pointers

First, consider replacing the raw_ptr with a smart pointer:

-   std::unique_ptr (See
    [example](https://docs.google.com/document/d/11YYsyPF9rQv_QFf982Khie3YuNPXV0NdhzJPojpZfco/edit?resourcekey=0-h1dr1uDzZGU7YWHth5TRAQ#heading=h.6itq8twigqt3))
-   scoped_refptr

#### Object vended from C API

In some cases, the object is vended by a C API. It means new/delete are not
used, but some equivalent API. In this case, it still makes sense to use a
unique_ptr, but with a custom deleter:
[example](https://chromium-review.googlesource.com/c/chromium/src/+/3650764).

For most common objects, a wrapper already exists:
[base::ScopedFILE](https://source.chromium.org/chromium/chromium/src/+/main:base/files/scoped_file.h;drc=898134d0d40dbbcd308e7d51655518ac7c6392b5;l=105),
[base::ScopedTempDir](https://source.chromium.org/chromium/chromium/src/+/main:base/files/scoped_temp_dir.h;l=25?q=ScopedTempDir&sq=&ss=chromium%2Fchromium%2Fsrc),
..

#### Object conditionally owned

In some cases, the raw_ptr conditionally owns the memory, depending on some
logic or some `is_owned` boolean. This can still use a unique_ptr
([example](https://chromium-review.googlesource.com/c/chromium/src/+/3829302))

```cpp
std::unique_ptr<T> t_owned_;
raw_ptr<T> t_; // Reference `t_owned_` or an external object.
```

#### Fallback solution

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

## I can't figure out which pointer is dangling

Usually this is a matter of straightforward reasoning, but should all else
fail, another option is to re-build with the alternative dangling pointer
detector as described in
[docs/dangling_ptr.md](./dangling_ptr.md#alternative-dangling-pointer-detector-experimental).
This will show the stacks for object creation, object destruction, and the
destruction of the object containing the dangling ptr member.

## FAQ - Why dangling pointers matter

Q. Gee, this is a raw pointer. Does destroying it actually do anything?

A. Yes. On some platforms `raw_ptr<T>` is a synonym for `T*`, but on many
platforms (more every day) `raw_ptr<T>` is the interface to PartitionAlloc’s
BackupRefPtr (BRP) Use-after-Free (UaF) protection mechanism. Destroying the
pointer thus actually performs internal bookkeeping and may also null
the pointer on destruction to trap use-after-destruct errors.

Q. So BRP mitigates these dangling pointers. What's the problem with just
keeping them if they are not used?

A. When an object is deleted under BRP, if there are any dangling references
remaining, the object must be quarantined and overwritten with a “zapped”
pattern as opposed to being simply freed. This costs cycles and memory
pressure. Even worse, it is still possible that the raw_ptr is converted to
`T*` and used with the zap value, or even used after the quarantine is
gone. Hence we are inventing mechanisms for finding dangling pointers so we
may remove the ones we know about. BRP will then make it harder to write
exploits with the ones we don’t know about in the wild.

Q. Why do we care if this is “just a test”?

A. Hitting dangling pointer warnings in tests blocks digging into actual
cases further in the code.

Q. Why should I think about lifetimes, anyway?

A. Holding an address that we aren’t allowed to de-reference is a bad
practice. It is a security and stability risk, a source of bugs that are
extremely difficult to diagnose, and a hazard for future coders. Also see
e.g. https://discourse.llvm.org/t/rfc-lifetime-annotations-for-c/61377 for
some ideas about how this might play out in the future.
