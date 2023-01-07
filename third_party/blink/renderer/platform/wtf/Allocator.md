# Memory management in Blink

This document gives a high-level overview of the memory management in Blink.

[TOC]

## Memory allocators

Blink objects are allocated by one of the following four memory allocators.

### Oilpan

Oilpan is a garbage collection system in Blink.
The lifetime of objects allocated by Oilpan is automatically managed.
The following objects are allocated by Oilpan:

* Objects that inherit from GarbageCollected<T>.

* HeapVector<T>, HeapHashSet<T>, HeapHashMap<T, U> etc

The implementation is in platform/heap/.
See [BlinkGCAPIReference.md](../heap/BlinkGCAPIReference.md) to learn the design.

### PartitionAlloc

PartitionAlloc is Chrome's default memory allocator. It's highly optimized for
performance and security.

When you simply call `new` or `malloc` PartitionAlloc will be used, unless
[specific gn args](/base/allocator/allocator/partition_allocator/build_config.md)
are specified.

The implementation is in /base/allocator/partition_allocator.
See [PartitionAlloc.md](/base/allocator/partition_allocator/PartitionAlloc.md)
to learn the design.

#### `USING_FAST_MALLOC`

Before PartitionAlloc become the default, `USING_FAST_MALLOC` had been used to
specify the allocator to be PartitionAlloc instead of malloc for an object.
Now `USING_FAST_MALLOC` only makes a difference when PartitionAlloc is disabled
in the build or [\*Scan](https://crbug.com/1277560) is enabled for blink.
For now `USING_FAST_MALLOC` is still preferred for non-GC blink objects that
allow `new`.

### Discardable memory

Discardable memory is a memory allocator that automatically discards
(not-locked) objects under memory pressure. Currently SharedBuffers
(which are mainly used as backing storage of Resource objects) are the only
user of the discardable memory.

The implementation is in src/base/memory/discardable_memory.*.
See [this document](https://docs.google.com/document/d/1aNdOF_72_eG2KUM_z9kHdbT_fEupWhaDALaZs5D8IAg/edit)
to learn the design.

## Basic allocation rules

In summary, Blink objects (except several special objects) should be allocated
using Oilpan or PartitionAlloc. malloc is discouraged.

The following is a basic rule to determine which of Oilpan or PartitionAlloc
you should use when allocating a new object:

* Use Oilpan if you want a GC to manage the lifetime of the object.
You need to make the object inherit from GarbageCollected<T>. See
[BlinkGCAPIReference.md](../heap/BlinkGCAPIReference.md) to learn
programming with Oilpan.

```c++
class X : public GarbageCollected<X> {
  ...;
};

void func() {
  X* x = new X;  // This is allocated by Oilpan.
}
```

* It's preferred to use USING_FAST_MALLOC if you don't need a GC to manage
the lifetime of the object (i.e., if scoped_refptr or unique_ptr is enough
to manage the lifetime of the object).

```c++
class X {
  USING_FAST_MALLOC(X);
  ...;
};

void func() {
  scoped_refptr<X> x = adoptRefPtr(new X);  // This is allocated by PartitionAlloc.
}
```

It is not a good idea to unnecessarily increase the number of objects
managed by Oilpan. Although Oilpan implements an efficient GC, the more objects
you allocate on Oilpan, the more pressure you put on Oilpan, leading to
a longer pause time.

Here is a guideline for when you ought to allocate an object using Oilpan,
and when you probably shouldn't:

* Use Oilpan for all script exposed objects (i.e., derives from
ScriptWrappable).

* Use Oilpan if managing its lifetime is usually simpler with Oilpan.
But see the next bullet.

* If the allocation rate of the object is very high, that may put unnecessary
strain on the Oilpan's GC infrastructure as a whole. If so and the object can be
allocated not using Oilpan without creating cyclic references or complex
lifetime handling, then use PartitionAlloc. For example, we allocate Strings
and LayoutObjects on PartitionAlloc.

For objects that don't need an operator new, you need to use either of the
following macros:

* If the object is only stack allocated, use STACK_ALLOCATED().

```c++
class X {
  STACK_ALLOCATED();
  ...;
};

void func() {
  X x;  // This is allowed.
  X* x2 = new X;  // This is forbidden.
}
```

* If the object can be allocated only as a part of object, use DISALLOW_NEW().

```c++
class X {
  DISALLOW_NEW();
  ...;
};

class Y {
  USING_FAST_MALLOC(Y);
  X x_;  // This is allowed.
  Vector<X> vector_;  // This is allowed.
};

void func() {
  X x;  // This is allowed.
  X* x = new X;  // This is forbidden.
}
```

Note that these macros are inherited. See a comment in wtf/allocator.h
for more details about the relationship between the macros and Oilpan.

If you have any question, ask oilpan-reviews@chromium.org.
