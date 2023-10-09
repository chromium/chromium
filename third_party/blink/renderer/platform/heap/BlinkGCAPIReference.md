# Oilpan - Blink GC

Oilpan is a garbage collector (GC) for Blink objects.
This document explores the design, API and usage of the GC.

[TOC]


## Overview

The general design of Oilpan is explained in the [Oilpan README](https://chromium.googlesource.com/v8/v8/+/main/include/cppgc/README.md).
This section focuses on the Blink specific extensions to that design.

## Threading model

Oilpan assumes heaps are not shared among threads.

Threads that want to allocate Oilpan objects must be *attached* to Oilpan, by calling either `blink::ThreadState::AttachMainThread()` or `blink::ThreadState::AttachCurrentThread()`.

Blink creates heaps and root sets (that directly refer to objects) per thread.
Matching a thread to its relevant heap is maintained by `blink::ThreadState`.

An object belongs to the thread it is allocated on.

Oilpan allows referring to objects from the same thread and cross-thread in various ways.
See [Handles](#Handles) for details.

## Heap partitioning

Blink assigns the following types to custom spaces in Oilpan's heap:
- Objects that are a collection backing are allocated in one of the collection backing *compactable* custom spaces.
- Objects that are a Node, a CSSValue or a LayoutObject are allocated in one of the typed custom spaces.

## Mode of operation

- Blink uses concurrent garbage collection (marking and sweeping), except for during thread termination and heap destruction.
- Blink tries to schedule GCs through the message loop where it is known that no objects are referred to from the native stack.
- When under memory pressure, Blink triggers a conservative GC on allocations.

# Oilpan API reference

## Base class templates

### GarbageCollected
<small>**Declared in:** `third_party/blink/renderer/platform/heap/garbage_collected.h`</small>

A class that wants the lifetime of its instances to be managed by Oilpan must inherit from `GarbageCollected<T>`.

```c++
class YourClass : public GarbageCollected<YourClass> {
  // ...
};
```

Instances of such classes are said to be *on Oilpan heap*, or *on heap* for short, while instances of other classes are called *off heap*.
In the rest of this document, the terms *on heap*, *on-heap objects* or *garbage-collected objects* are used to mean the objects on Oilpan heap instead of on normal (default) dynamic allocator's heap space.

Instances of a garbage-collected class can be created/allocated through `MakeGarbageCollected<T>`.
Declaring a class as garbage collected disallows the use of `operator new`.
It is not allowed to free garbage-collected object with `delete`, as Oilpan is responsible for deallocating the object once it determines the object is unreachable.

Garbage-collected objects may not be allocated on stack. See [STACK_ALLOCATED](#STACK_ALLOCATED) and [Heap collections](#Heap-collections) for exceptions to this rule.

Garbage-collected class may need to have a tracing method. See [Tracing](#Tracing) for details.

Non-trivially destructible classes will be automatically finalized.
Non-final classes that are not trivially destructible are required to have a virtual destructor.
Trivially destructible classes should not have a destructor.
Adding a destructor to such classes would make them non-trivially destructible and would hinder performance.
Note that finalization is done at an arbitrary time after the object becomes unreachable.
Any destructor executed within the finalization period *must not* touch any other on-heap object because destructors can be executed in any order.

`GarbageCollected<T>` or any class deriving from `GarbageCollected<T>`, directly or indirectly, must be the first element in its base class list (called "leftmost derivation rule").
This rule is needed to assure each on-heap object has its own canonical address.

```c++
class A : public GarbageCollected<A>, public P {
  // OK: GarbageCollected<A> is leftmost.
};

class B : public A, public Q {
  // OK: A is leftmost.
};

// class C : public R, public A {
//   BAD: A must be the first base class.
//        If R is also garbage collected, A should be [GarbageCollectedMixin](#GarbageCollectedMixin).
// };
```

If a non-leftmost base class needs to retain an on-heap object, that base class needs to inherit from [GarbageCollectedMixin](#GarbageCollectedMixin).
It's generally recommended to make *any* non-leftmost base class inherit from `GarbageCollectedMixin` because it's dangerous to save a pointer to a non-leftmost non-`GarbageCollectedMixin` subclass of an on-heap object.

```c++
P* raw_pointer;
void someFunction(P* p) {
  ...
  raw_pointer = p;
  ...
}

class A : public GarbageCollected<A>, public P {
public:
  void someMemberFunction()
  {
    someFunction(this); // DANGEROUS, a raw pointer to an on-heap object. Object might be collected, resulting in a dangling pointer and possible memory corruption.
  }
};
```

### GarbageCollectedMixin
<small>**Declared in:** `third_party/blink/renderer/platform/heap/garbage_collected.h`</small>

A non-leftmost base class of a garbage-collected class should derive from `GarbageCollectedMixin`.

A class deriving from `GarbageCollectedMixin` can be treated similarly as garbage-collected classes.
Specifically, it can have `Member<T>`s and `WeakMember<T>`s, and a tracing method.
A pointer to such a class must be retained in the same smart pointer wrappers as a pointer to a garbage-collected class, such as `Member<T>` or `Persistent<T>`.
The [tracing](#Tracing) method of a garbage-collected class, if any, must contain a delegating call for each mixin base class.

```c++
class P : public GarbageCollectedMixin {
 public:
  // OK: Needs to trace q_.
  virtual void Trace(Visitor* visitor) const { visitor->Trace(q_); }
 private:
  // OK: Allowed to have Member<T>.
  Member<Q> q_;
};

class A final : public GarbageCollected<A>, public P {
 public:
  // Delegating call for P is needed.
  virtual void Trace(Visitor* visitor) const { P::Trace(visitor); }
};
```

A class that is a descendant of `GarbageCollectedMixin` but not a descendant of `GarbageCollected<T>` cannot be instantiated..

```c++
class P : public GarbageCollectedMixin { };
class Q : public GarbageCollectedMixin { };
class R : public Q { };

class A : public GarbageCollected<A>, public P, public R {
  // OK: Resolving pure virtual functions of P and R.
};

class B : public GarbageCollected<B>, public P {
  // OK: Different garbage-collected classes may inherit from the same mixin (P).
};

void foo() {
  MakeGarbageCollected<A>();    // OK: A can be instantiated.
  // MakeGarbageCollected<R>(); // BAD: R has pure virtual functions.
}
```


## Class properties

### USING_PRE_FINALIZER
<small>**Declared in:** `third_party/blink/renderer/platform/heap/prefinalizer.h`</small>

`USING_PRE_FINALIZER(ClassName, FunctionName)` in a class declaration declares the class has a *pre-finalizer* of name `FunctionName`.
A pre-finalizer must have the function signature `void()` but can have any name.

A pre-finalizer is a user-defined member function of a garbage-collected class that is called when the object is going to be reclaimed.
It is invoked before the sweeping phase starts to allow a pre-finalizer to touch any other on-heap objects which is forbidden from destructors.
It is useful for doing cleanups that cannot be done with a destructor.

```c++
class YourClass : public GarbageCollected<YourClass> {
  USING_PRE_FINALIZER(YourClass, Dispose);
public:
  void Dispose() {
    // OK: Other on-heap objects can be touched in a pre-finalizer.
    other_->Dispose();
  }

  ~YourClass() {
    // BAD: Not allowed.
    // other_->Dispose();
  }

private:
  Member<OtherClass> other_;
};
```
Sometimes it is necessary to further delegate pre-finalizers up the class hierarchy, similar to how destructors destruct in reverse order wrt. to construction.
It is possible to construct such delegations using virtual methods.

```c++
class Parent : public GarbageCollected<Parent> {
  USING_PRE_FINALIZER(Parent, Dispose);
 public:
  void Dispose() { DisposeImpl(); }

  virtual void DisposeImpl() {
    // Pre-finalizer for {Parent}.
  }
  // ...
};

class Child : public Parent {
 public:
  void DisposeImpl() {
    // Pre-finalizer for {Child}.
    Parent::DisposeImpl();
  }
  // ...
};
```

Alternatively, several classes in the same hierarchy may have pre-finalizers and these will also be executed in reverse order wrt. to construction.

```c++
class Parent : public GarbageCollected<Parent> {
  USING_PRE_FINALIZER(Parent, Dispose);
 public:
  void Dispose() {
    // Pre-finalizer for {Parent}.
  }
  // ...
};

class Child : public Parent {
  USING_PRE_FINALIZER(Child, Dispose);
 public:
  void Dispose() {
    // Pre-finalizer for {Child}.
  }
  // ...
};
```

*Notes*
- Pre-finalizers are not allowed to resurrect objects (i.e. they are not allowed to relink dead objects into the object graph).
- Pre-finalizers have some implications on the garbage collector's performance: the garbage-collector needs to iterate all registered pre-finalizers at every GC.
Therefore, a pre-finalizer should be avoided unless it is really necessary.
Especially, avoid defining a pre-finalizer in a class that might be allocated a lot.

### DISALLOW_NEW
<small>**Declared in:** `third_party/blink/renderer/platform/wtf/allocator/allocator.h`</small>

Class-level annotation declaring the class cannot be separately allocated using `operator new` or `MakeGarbageCollected<T>`.
It can be used on stack, as an embedded part of some other object, or as a value in a heap collection.
If the class has `Member<T>` references, it needs a `Trace()` method, which must be called by the embedding object (e.g. the collection or the object that this class is a part of).

Classes with this annotation need a `Trace()` method, but should not inherit a garbage collected class.

### STACK_ALLOCATED
<small>**Declared in:** `third_party/blink/renderer/platform/wtf/allocator/allocator.h`</small>

Class level annotation that should be used if the class is only stack allocated.
Any fields holding garbage-collected objects should use raw pointers or references.

Classes with this annotation do not need to define a `Trace()` method as they are on the stack, and are automatically traced and kept alive should a conservative GC be required.

Stack allocated classes must not inherit a on-heap garbage collected class.

Marking a class as STACK_ALLOCATED implicitly implies [DISALLOW_NEW](#DISALLOW_NEW), and thus disallow the use of `operator new` and `MakeGarbageCollected<T>`.


## Handles

Class templates in this section are smart pointers, each carrying a pointer to an on-heap object (think of `scoped_refptr<T>` for `RefCounted<T>`, or `std::unique_ptr`).
Collectively, they are called *handles*.

On-heap objects must be retained by any of these, depending on the situation.

### Raw pointers

Using raw pointers to garbage-collected objects on-heap is forbidden and will yield in memory corruptions.
On-stack references to garbage-collected object (including function parameters and return types), on the other hand, must use raw pointers.
This is the only case where raw pointers to on-heap objects should be used.

```c++
void Foo() {
  SomeGarbageCollectedClass* object = MakeGarbageCollected<SomeGarbageCollectedClass>(); // OK, retained by a raw pointer.
  // ...
}
// OK to leave the object behind. The GC will reclaim it when it becomes unused.
```

### Member, WeakMember
<small>**Declared in:** `third_party/blink/renderer/platform/heap/member.h`</small>

In a garbage-collected class, on-heap objects must be retained by `Member<T>` or `WeakMember<T>`, for strong or weak traced semantics, respectively.
Both references may only refer to objects that are owned by the same thread.

`Member<T>` represents a *strong* reference to an object of type `T`, which means that the referred object is kept alive as long as the owner class instance is alive.
Unlike `scoped_refptr<T>`, it is okay to form a reference cycle with members (in on-heap objects) and raw pointers (on stack).

`WeakMember<T>` is a *weak* reference to an object of type `T`.
Unlike `Member<T>`, `WeakMember<T>` does not keep the pointed object alive.
The pointer in a `WeakMember<T>` can become `nullptr` when the object gets garbage-collected.
It may take some time for the pointer in a `WeakMember<T>` to become `nullptr` after the object actually becomes unreachable, because this rewrite is only done within Oilpan's garbage collection cycle.

```c++
class SomeGarbageCollectedClass : public GarbageCollected<SomeGarbageCollectedClass> {
  ...
private:
  Member<AnotherGarbageCollectedClass> another_; // OK, retained by Member<T>.
  WeakMember<AnotherGarbageCollectedClass> anotherWeak_; // OK, weak reference.
};
```
It is required that every `Member<T>` and `WeakMember<T>` in a garbage-collected class be traced. See [Tracing](#Tracing).

The use of `WeakMember<T>` incurs some overhead in garbage collector's performance.
Use it sparingly.
Usually, weak members are not necessary at all, because reference cycles with members are allowed.
More specifically, `WeakMember<T>` should be used only if the owner of a weak member can outlive the pointed object.
Otherwise, `Member<T>` should be used.

Generally `WeakMember<T>` requires to be converted into a strong reference to avoid reclamation until access.
The following example uses a raw pointer to create a strong reference on stack.
```c++
void Foo(WeakMember<T>& weak_object) {
  T* proxy = weak_object.Get();  // OK, proxy will keep weak_object alive.
  if (proxy) {
    GC();
    proxy->Bar();
  }
}
```

### UntracedMember
<small>**Declared in:** `third_party/blink/renderer/platform/heap/member.h`</small>

`UntracedMember<T>` represents a reference to a garbage collected object which is ignored by Oilpan.
The reference may only refer to objects that are owned by the same thread.

Unlike `Member<T>`, `UntracedMember<T>` will not keep an object alive.
However, unlike `WeakMember<T>`, the reference will not be cleared (i.e. set to `nullptr`) if the referenced object dies.
Furthermore, class fields of type `UntracedMember<T>` should not be traced by the class' tracing method.

Users should avoid using `UntracedMember<T>` in any case other than when implementing [custom weakness semantics](#Custom-weak-callbacks).

### Persistent, WeakPersistent
<small>**Declared in:** `third_party/blink/renderer/platform/heap/persistent.h`</small>

In a non-garbage-collected class, on-heap objects must be retained by `Persistent<T>`, or `WeakPersistent<T>`.
Both references may only refer to objects that are owned by the same thread.

`Persistent<T>` is the most basic handle in the persistent family, which makes the referred object alive
unconditionally, as long as the persistent handle is alive.

`WeakPersistent<T>` does not keep the referred object alive, and becomes `nullptr` when the object gets
garbage-collected, just like `WeakMember<T>`.

```c++
#include "third_party/blink/renderer/platform/heap/persistent.h"
...
class NonGarbageCollectedClass {
  ...
private:
  Persistent<SomeGarbageCollectedClass> something_; // OK, the object will be alive while this persistent is alive.
};
```

Persistents have a small unavoidable overhead because they require maintaining a list of all persistents.
Therefore, creating or keeping a lot of persistents at once would have performance implications and should be avoided when possible.

Weak variants have overhead just like `WeakMember<T>`.
Use them sparingly.

***
**Warning:** `Persistent<T>` is vulnerable to reference cycles.
If a reference cycle is formed with `Persistent`s, `Member`s, `RefPtr`s and `OwnPtr`s, all the objects in the cycle **will leak**, since
no object in the cycle can be aware of whether they are ever referred to from outside the cycle.

Be careful not to create a reference cycle when adding a new persistent.
If a cycle is inevitable, make sure the cycle is eventually cut and objects become reclaimable.
***

### CrossThreadPersistent, CrossThreadWeakPersistent
<small>**Declared in:** `third_party/blink/renderer/platform/heap/cross_thread_persistent.h`</small>

`CrossThreadPersistent<T>` and `CrossThreadWeakPersistent<T>` are cross-thread variants of `Persistent<T>` and
`WeakPersistent<T>`, respectively, which can point to an object owned by a different thread (i.e. in a different heap).

In general, Blink does not support shared-memory concurrency designs.
The need of cross-thread persistents may indicate a poor design in multi-thread object ownership.
Think twice if they are really necessary.

Similar to `Persistent<T>`, `CrossThreadPersistent<T>` is prone to creating cycles that results in memory leaks.

### CrossThreadHandle, CrossThreadWeakHandle
<small>**Declared in:** `third_party/blink/renderer/platform/heap/cross_thread_handle.h`</small>

`CrossThreadHandle<T>` and `CrossThreadWeakHandle<T>` are cross-thread variants of `Persistent<T>` and
`WeakPersistent<T>`, respectively, which can point to an object owned by a different thread (i.e. in a different heap).
The difference to `CrossThreadPersistent<T>` is that these handles prohibit deref on any thread except the thread that owns the object pointed to.
In other words, CrossThreadHandle<T> only support thread-safe copy, move, and destruction.

The main use case for `CrossThreadHandle<T>` is delegating work to some other thread while retaining callbacks or other objects that are eventually used from the originating thread.

Similar to `Persistent<T>`, `CrossThreadHandle<T>` is prone to creating cycles that results in memory leaks.


## Tracing

A garbage-collected class is required to have *a tracing method*, which lists all the references to on-heap objects it has.
The tracing method is called when the garbage collector needs to determine (1) all the on-heap objects referred from a
live object, and (2) all the weak handles that may be filled with `nullptr` later.

The basic form of tracing is illustrated below:

```c++
// In a header file:
class SomeGarbageCollectedClass final
    : public GarbageCollected<SomeGarbageCollectedClass> {
 public:
  void Trace(Visitor*) const;

private:
  Member<AnotherGarbageCollectedClass> another_;
};

// In an implementation file:
void SomeGarbageCollectedClass::Trace(Visitor* visitor) const {
  visitor->Trace(another_);
}
```

Specifically, if a class needs a tracing method, declaring and defining a `Trace(Visitor*) const` method is required.
This method is normally declared in the header file and defined once in the implementation file, but there are variations.
A common variation is to declare a virtual `Trace()` for base classes that will be subclassed.

The function implementation must contain:
- For each reference tp an on-heap object `object` in the class, a tracing call: `visitor->Trace(object);`.
- For each embedded `DISALLOW_NEW` object `object` in the class, a tracing call: `visitor->Trace(object);`.
- For each base class of the class `BaseClass` that is a descendant of `GarbageCollected<T>` or `GarbageCollectedMixin`, a delegation call to base class: `BaseClass::Trace(visitor)`.
It is recommended that the delegation call, if any, is put at the end of a tracing method.
- See [Advanced weak handling](#Advanced%20Weak%20Handling) for implementing non-trivial weakness.

The following example shows more involved usage:

```c++
class A : public GarbageCollected<A> {
 public:
  virtual void Trace(Visitor*) const {} // Nothing to trace here.
};

class B : public A {
  // Nothing to trace here; exempted from having a tracing method.
};

class C final : public B {
 public:
  void Trace(Visitor*) const final;

 private:
  Member<X> x_;
  WeakMember<Y> y_;
  HeapVector<Member<Z>> z_;
};

void C::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_); // Weak member needs to be traced.
  visitor->Trace(z_); // Heap collection does, too.
  B::Trace(visitor); // Delegate to the parent. In this case it's empty, but this is required.
}
```

Generally, a `Trace()` method would be just a sequence of delegations of the form `visitor->Trace(object);` for fields or `BaseClass::Trace(visitor)` for base classes.
It should avoid more complex logic such as branches or iterations.
In case a branch is unavoidable, the branch condition should depend only on const fields of the object.
In case an iteration is unavoidable, the number of iterations and the location of the buffer that is iterated should also depend only on const fields of the object.
Using non-const fields in `Trace()` methods may cause data races and other issues in the GC.

## Heap collections

Oilpan, like any other managed runtime library, provides basic support for collections that integrate its managed types `Member<T>` and `WeakMember<T>`.
Do not use heap collection with persistent types (e.g. `HeapVector<Persistent<T>>`).

Collections compared to other libraries used in Blink:

| stdlib             | WTF                 | Oilpan                    |
| ------------------ | ------------------- | ------------------------- |
| std::vector        | WTF::Vector         | blink::HeapVector         |
| std::deque         | WTF::Deque          | blink::HeapDeque          |
| std::unordered_map | WTF::HashMap        | blink::HeapHashMap        |
| std::unordered_set | WTF::HashSet        | blink::HeapHashSet        |
| -                  | WTF::LinkedHashSet  | blink::HeapLinkedHashSet  |
| -                  | WTF::HashCountedSet | blink::HeapHashCountedSet |

These heap collections work mostly the same way as their stdlib or WTF collection counterparts but there are some things to keep in mind.
Heap collections are regular heap objects and thus must be properly traced from a `Trace` method.
They can also be inlined into other objects for performance reasons and are allowed to be directly used on stack.

```c++
class A final : public GarbageCollected<A> {
 public:
  void Trace(Visitor* visitor) const { visitor->Trace(vec_); }
 private:
  HeapVector<Member<B>> vec_;
};
```

Like any other object, they may be referred to from a non-garbage-collected class using `Persistent`.

```c++
class NonGCed final {
 private:
  Persistent<HeapVector<Member<B>>> vec_;
};
```

## Advanced weak handling

In addition to basic weak handling using `WeakMember<T>` Oilpan also supports:
- Weak collections
- Custom weak callbacks

### Weak collections

Like regular weakness, collections support weakness by putting references in `WeakMember<T>`.

In associative containers such as `HeapHashMap` or `HeapHashSet` Oilpan distinguishes between *pure weakness* and *mixed weakness*:
- Pure weakness: All entries in such containers are wrapped in `WeakMember<T>`.
  Examples are `HeapHashSet<WeakMember<T>>` and `HeapHashMap<WeakMember<T>, WeakMember<U>>`.
- Mixed weakness: Only some entries in such containers are wrapped in `WeakMember<T>`.
  This can only happen in `HeapHashMap`.
  Examples are `HeapHashMap<WeakMember<T>, Member<U>>, HeapHashMap<Member<T>, WeakMember<U>>, HeapHashMap<WeakMember<T>, int>, and HeapHashMap<int, WeakMember<T>>.
  Note that in the last example the type `int` is traced even though it does not support tracing.

The semantics then are as follows:
- Pure weakness: Oilpan will automatically remove the entries from the container if any of its declared `WeakMember<T>` fields points to a dead object.
- Mixed weakness: Oilpan applies ephemeron semantics for mixed weakness with `WeakMember<T>` and `Member<T>`. Mixing `WeakMember<T>` with non-tracable types results in pure weakness treatment for the corresponding entry.

Weak references (e.g. `WeakMember<T>`) are not supported in sequential containers such as `HeapVector` or `HeapDeque`.

Iterators to weak collections keep their collections alive strongly.
This allows for the GC to kick in during weak collection iteration.

```c++
template <typename Callback>
void IterateAndCall(HeapHashMapM<WeakMember<T>, WeakMember<U>>* hash_map, Callback callback) {
  for (auto pair& : *hash_map) {
    callback(pair.first, pair.second);  // OK, will invoke callback(WeakMember<T>, WeakMember<U>).
  }
}
```

### Custom weak callbacks

In case very specific weakness semantics are required Oilpan allows adding custom weakness callbacks through its tracing method.

There exist two helper methods on `blink::Visitor` to add such callbacks:
- `RegisterWeakCallback`: Used to add custom weak callbacks of the form `void(void*, const blink::LivenessBroker&)`.
- `RegisterWeakCallbackMethod`: Helper for adding an instance method.

Note that custom weak callbacks should not be used to clear `WeakMember<T>` fields as such fields are automatically handled by Oilpan.
Instead, for managed fields that require custom weakness semantics, users should wrap such fields in `UntracedMember<T>` indicating that Oilpan is ignoring those fields.

The following example shows how this can be used:

```c++

class W final : public GarbageCollected<W> {
 public:
  virtual void Trace(Visitor*) const;
 private:
  void ProcessCustomWeakness(const LivenessBroker&);

  UntracedMember<C> other_;
};

void W::Trace(Visitor* visitor) const {
  visitor->template RegisterCustomWeakMethod<W, &W::ProcessCustomWeakness>(this);
}

void W::ProcessCustomWeakness(const LivenessBroker& info) {
  if (info.IsHeapObjectAlive(other_)) {
    // Do something with other_.
  }
  other_ = nullptr;
}
```

Note that the custom weakness callback in this example is only executed if `W` is alive and properly traced.
If `W` itself dies than the callback will not be executed.
Operations that must always happen should instead go into destructors or pre-finalizers.

## Traits helpers
<small>**Declared in:** `third_party/blink/renderer/platform/heap/heap_traits.h`</small>

At times, one may be working on code that needs to deal with both, off heap and on heap, objects.
The following helpers can aid in writing code that needs to use different wrappers and containers based on whether a type is managed by Oilpan.

### AddMemberIfNeeded<T>

Given a type `T`, defines a type alias that is either `Member<T>` or `T` depending on whether `T` is a type managed by Oilpan.

```c++
class A final : public GarbageCollected<A> {};
class B final {};

AddMemberIfNeeded<B> v1;       // B v1;
AddMemberIfNeeded<int32_t> v2; // int32_t v2;
AddMemberIfNeeded<A> v3;       // Member<A> v3;
```

### VectorOf<T>

Given a type `T`, defines a type alias that is either `HeapVector<T>`, `HeapVector<Member<T>>` or `Vector<T>` based on the following rules:

* `T` is a garbage collected type managed by Oilpan → `HeapVector<Member<T>>`
* `T` has a `Trace()` method but is not managed by Oilpan → `HeapVector<T>` (this is a rare case; IDL unions and dictionaries fall in this category, for example)
* All other cases → `Vector<T>`

```c++
class MyGarbageCollectedClass : public GarbageCollected<MyGarbageCollectedClass> {
  // ...
};

class MyNonGCButTraceableClass {
 public:
  void Trace(Visitor* visitor) const {
    // ...
  }
};

class MyNotGarbageCollectedClass {
  // ...
};

VectorOf<float> v1;                       // Vector<float> v1;
VectorOf<MyNotGarbageCollectedClass> v2;  // Vector<MyNotGarbageCollectedClass> v2;
VectorOf<MyNonGCButTraceableClass> v3;    // HeapVector<MyNonGCButTraceableClass> v3;
VectorOf<MyGarbageCollectedClass> v4;     // HeapVector<Member<MyGarbageCollectedClass>> v4;
```

### VectorOfPairs<T, U>

Similar to `VectorOf<T>`, but defines a type alias that is either `HeapVector<std::pair<V, X>>` (where `V` is either `T` or `Member<T>` and `X` is either `U` or `Member<U>`) or `Vector<std::pair<T, U>>`.

In other words, if either `T` or `U` needs to be wrapped in a `HeapVector` instead of a `Vector`, `VectorOfPairs` will use a `HeapVector<std::pair<>>` and wrap them with `Member<>` appropriately.
Otherwise, a `Vector<std::pair<>>` will be used.

```c++
class MyGarbageCollectedClass : public GarbageCollected<MyGarbageCollectedClass> {
  // ...
};

class MyNonGCButTraceableClass {
 public:
  void Trace(Visitor* visitor) const {
    // ...
  }
};

class MyNotGarbageCollectedClass {
  // ...
};

// Vector<std::pair<double, int8_t>> v1;
VectorOfPairs<double, int8_t> v1;

// Vector<std::pair<MyNotGarbageCollectedClass, String>> v2;
VectorOfPairs<MyNotGarbageCollectedClass, String> v2;

// HeapVector<std::pair<float, MyNonGCButTraceableClass>> v3;
VectorOfPairs<float, MyNonGCButTraceableClass> v3;

// HeapVector<std::pair<MyNonGCButTraceableClass, Member<MyGarbageCollectedClass>>> v4;
VectorOfPairs<MyNonGCButTraceableClass, MyGarbageCollectedClass> v4;
```
