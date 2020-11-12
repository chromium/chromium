# Blink GC API reference

This document describes the usage of Oilpan -- Blink's garbage collector.
If you just want to get an overview of the API you can have a look at
[this tutorial](https://docs.google.com/presentation/d/1XPu03ymz8W295mCftEC9KshH9Icxfq81YwIJQzQrvxo/edit#slide=id.p).
If you're interested in wrapper tracing, see [Wrapper Tracing Reference](../bindings/TraceWrapperReference.md).

[TOC]

## Header file

Unless otherwise noted, any of the primitives explained on this page require the following `#include` statement:

```c++
#include "third_party/blink/renderer/platform/heap/handle.h"
```

## Base class templates

### GarbageCollected

A class that wants the lifetime management of its instances to be managed by Oilpan, it must inherit from `GarbageCollected<T>`.

```c++
class YourClass : public GarbageCollected<YourClass> {
  // ...
};
```

Instances of such classes are said to be *on Oilpan heap*, or *on heap* for short, while instances of other classes are called *off heap*.
In the rest of this document, the terms *on heap* or *on-heap objects* are used to mean the objects on Oilpan heap instead of on normal (default) dynamic allocator's heap space.

You can create an instance of your class through `MakeGarbageCollected<T>`, while you may not free the object with `delete`, as Oilpan is responsible for deallocating the object once it determines the object is unreachable.

You may not allocate an on-heap object on stack.

Your class may need to have a tracing method. See [Tracing](#Tracing) for details.

Your class will be automatically finalized as long as it is non-trivially destructible.
Non-final classes that are not trivially destructible are required to have a virtual destructor.
Trivially destructible classes should not have a destructor. Adding a destructor to such classes would make then non-trivially destructible and would hinder performance.
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

If a non-leftmost base class needs to retain an on-heap object, that base class needs to inherit from [GarbageCollectedMixin](#GarbageCollectedMixin). It's generally recommended to make *any* non-leftmost base class inherit from `GarbageCollectedMixin` because it's dangerous to save a pointer to a non-leftmost non-`GarbageCollectedMixin` subclass of an on-heap object.

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

You cannot instantiate a class that is a descendant of `GarbageCollectedMixin` but not a descendant of `GarbageCollected<T>`.

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

*Notes*
- Pre-finalizers are not allowed to allocate new on-heap objects or resurrect objects (i.e., they are not allowed to relink dead objects into the object graph).
- Pre-finalizers have some implications on the garbage collector's performance: the garbage-collector needs to iterate all registered pre-finalizers at every GC.
Therefore, a pre-finalizer should be avoided unless it is really necessary.
Especially, avoid defining a pre-finalizer in a class that can be allocated a lot.

### STACK_ALLOCATED

Class level annotation that should be used if the object is only stack allocated; it disallows use of `operator new`. Any fields holding garbage-collected objects should use regular pointers or references and you do not need to define a `Trace()` method as they are on the stack, and automatically traced and kept alive should a conservative GC be required.

Classes with this annotation do not need a `Trace()` method and must not inherit a on-heap garbage collected class.

Marking a class as STACK_ALLOCATED implicitly implies [DISALLOW_NEW](#DISALLOW_NEW).

### DISALLOW_NEW

Class-level annotation declaring the class cannot be separately allocated using `operator new`.
It can be used on stack, as a part of object, or as a value in a heap collection.
If the class has `Member<T>` references, you need a `Trace()` method which the object containing the `DISALLOW_NEW()`
part object must call upon. The clang Blink GC plugin checks and enforces this.

Classes with this annotation need a `Trace()` method, but should not inherit a garbage collected class.


## Handles

Class templates in this section are smart pointers, each carrying a pointer to an on-heap object (think of `scoped_refptr<T>`
for `RefCounted<T>`). Collectively, they are called *handles*.

On-heap objects must be retained by any of these, depending on the situation.

### Raw pointers

Raw pointers to garbage-collected objects should be avoided as they may cause memory corruptions.
An exception to this rule is on-stack references to on-heap objects (including function parameters and return types) which must be raw pointers.

```c++
void someFunction() {
  SomeGarbageCollectedClass* object = MakeGarbageCollected<SomeGarbageCollectedClass>(); // OK, retained by a pointer.
  ...
}
// OK to leave the object behind. The Blink GC system will free it up when it becomes unused.
```

### Member, WeakMember

In a garbage-collected class, on-heap objects must be retained by `Member<T>` or `WeakMember<T>`, depending on
the desired semantics.

`Member<T>` represents a *strong* reference to an object of type `T`, which means that the referred object is kept
alive as long as the owner class instance is alive. Unlike `scoped_refptr<T>`, it is okay to form a reference cycle with
members (in on-heap objects) and raw pointers (on stack).

`WeakMember<T>` is a *weak* reference to an object of type `T`. Unlike `Member<T>`, `WeakMember<T>` does not keep
the pointed object alive. The pointer in a `WeakMember<T>` can become `nullptr` when the object gets garbage-collected.
It may take some time for the pointer in a `WeakMember<T>` to become `nullptr` after the object actually goes unused,
because this rewrite is only done within Blink GC's garbage collection period.

```c++
class SomeGarbageCollectedClass : public GarbageCollected<SomeGarbageCollectedClass> {
  ...
private:
  Member<AnotherGarbageCollectedClass> another_; // OK, retained by Member<T>.
  WeakMember<AnotherGarbageCollectedClass> anotherWeak_; // OK, weak reference.
};
```

The use of `WeakMember<T>` incurs some overhead in garbage collector's performance. Use it sparingly. Usually, weak
members are not necessary at all, because reference cycles with members are allowed.

More specifically, `WeakMember<T>` should be used only if the owner of a weak member can outlive the pointed object.
Otherwise, `Member<T>` should be used.

You need to trace every `Member<T>` and `WeakMember<T>` in your class. See [Tracing](#Tracing).

### UntracedMember

`UntracedMember<T>` represents a reference to a garbage collected object which is ignored by Oilpan.

Unlike 'Member<T>', 'UntracedMember<T>' will not keep an object alive. However, unlike 'WeakMember<T>', the reference will not be cleared (i.e. set to 'nullptr') if the referenced object dies.
Furthermore, class fields of type 'UntracedMember<T>' should not be traced by the class' tracing method.

Users should  use 'UntracedMember<T>' when implementing [custom weakness semantics](#Custom-weak-callbacks).

### Persistent, WeakPersistent, CrossThreadPersistent, CrossThreadWeakPersistent

In a non-garbage-collected class, on-heap objects must be retained by `Persistent<T>`, `WeakPersistent<T>`,
`CrossThreadPersistent<T>`, or `CrossThreadWeakPersistent<T>`, depending on the situations and the desired semantics.

`Persistent<T>` is the most basic handle in the persistent family, which makes the referred object alive
unconditionally, as long as the persistent handle is alive.

`WeakPersistent<T>` does not make the referred object alive, and becomes `nullptr` when the object gets
garbage-collected, just like `WeakMember<T>`.

`CrossThreadPersistent<T>` and `CrossThreadWeakPersistent<T>` are cross-thread variants of `Persistent<T>` and
`WeakPersistent<T>`, respectively, which can point to an object in a different thread.

```c++
#include "third_party/blink/renderer/platform/heap/persistent.h"
...
class NonGarbageCollectedClass {
  ...
private:
  Persistent<SomeGarbageCollectedClass> something_; // OK, the object will be alive while this persistent is alive.
};
```

`persistent.h` provides these persistent pointers.

*** note
**Warning:** `Persistent<T>` and `CrossThreadPersistent<T>` are vulnerable to reference cycles. If a reference cycle
is formed with `Persistent`s, `Member`s, `RefPtr`s and `OwnPtr`s, all the objects in the cycle **will leak**, since
nobody in the cycle can be aware of whether they are ever referred from anyone.

When you are about to add a new persistent, be careful not to create a reference cycle. If a cycle is inevitable, make
sure the cycle is eventually cut by someone outside the cycle.
***

Persistents have small overhead in itself, because they need to maintain the list of all persistents. Therefore, it's
not a good idea to create or keep a lot of persistents at once.

Weak variants have overhead just like `WeakMember<T>`. Use them sparingly.

The need of cross-thread persistents may indicate a poor design in multi-thread object ownership. Think twice if they
are really necessary.

## Tracing

A garbage-collected class is required to have *a tracing method*, which lists up all the on-heap objects it has.
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

Specifically, if your class needs a tracing method, you need to declare and define a `Trace(Visitor*) const` method.
This method is normally declared in the header file and defined once in the implementation file, but there are variations.
Another common variation is to declare a virtual `Trace()` for base classes that will be subclassed.

The function implementation must contain:
- For each on-heap object `object` in your class, a tracing call: `visitor->Trace(object);`.
- For each base class of your class `BaseClass` that is a descendant of `GarbageCollected<T>` or `GarbageCollectedMixin`, a delegation call to base class: `BaseClass::Trace(visitor)`.
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

## Heap collections

Oilpan, like any other managed runtime library, provides basic support for collections that integrate its managed types `Member<T>` and `WeakMember<T>`.
Do not use heap collection with persistent types (e.g. HeapVector<Persistent<T>>).

Collections compared to other libraries used in Blink:

| stdlib             | WTF                 | Oilpan                    |
| ------------------ | ------------------- | ------------------------- |
| std::vector        | WTF::Vector         | blink::HeapVector         |
| std::deque         | WTF::Deque          | blink::HeapDeque          |
| std::unordered_map | WTF::HashMap        | blink::HeapHashMap        |
| std::unordered_set | WTF::HashSet        | blink::HeapHashSet        |
| -                  | WTF::LinkedHashSet  | blink::HeapLinkedHashSet  |
| -                  | WTF::ListHashSet    | blink::HeapListHashSet    |
| -                  | WTF::HashCountedSet | blink::HeapHashCountedSet |

These heap collections work mostly the same way as their stdlib or WTF collection counterparts but there are some things to keep in mind.
Heap collections do not inherit from `GarbageCollected` but are nonetheless allocated on-heap and thus must be properly traced from a `Trace` method.

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

In sequence containers such as `HeapVector` the `WeakMember<T>` references are just cleared without adding any additional handling (cleared references are not automatically removed from the container).

In associative containers such as `HeapHashMap` or `HeapHashSet` Oilpan distinguishes between *pure weakness* and *mixed weakness*:
- Pure weakness: All entries in such containers are wrapped in `WeakMember<T>`.
  Examples are `HeapHashSet<WeakMember<T>>` and `HeapHashMap<WeakMember<T>, WeakMember<U>>`.
- Mixed weakness: Only some entries in such containers are wrapped in `WeakMember<T>`.
  This can only happen in `HeapHashMap`.
  Examples are `HeapHashMap<WeakMember<T>, Member<U>>, HeapHashMap<Member<T>, WeakMember<U>>, HeapHashMap<WeakMember<T>, int>, and HeapHashMap<int, WeakMember<T>>.
  Note that in the last example the type `int` is traced even though it does not support tracing.

The semantics then are as follows:
- Pure weakness: Oilpan will automatically remove the entries from the container if any of its declared `WeakMember<T>` fields points to a dead object.
- Mixed weakness: Oilpan applies ephemeron semantics meaning that the strong parts of an entry are only treated as strong if the `WeakMember<T>` fields point to a live object.

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

* `T` is a type managed by the Blink GC → `HeapVector<Member<T>>`
* `T` has a `Trace()` method but is not managed by the Blink GC → `HeapVector<T>` (this is a rare case; IDL unions and dictionaries fall in this category, for example)
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

In other words, if either `T` or `U` needs to be wrapped in a `HeapVector` instead of a `Vector`, `VectorOfPairs` will use a `HeapVector<std::pair<>>` and wrap them with `Member<>` appropriately. Otherwise, a `Vector<std::pair<>>` will be used.

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
