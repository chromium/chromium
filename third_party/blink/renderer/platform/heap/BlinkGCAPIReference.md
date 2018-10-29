# Blink GC API reference

This is a through document for Oilpan API usage.
If you want to learn the API usage quickly, look at
[this tutorial](https://docs.google.com/presentation/d/1XPu03ymz8W295mCftEC9KshH9Icxfq81YwIJQzQrvxo/edit#slide=id.p).
If you're just interested in wrapper tracing,
see [Wrapper Tracing Reference](../bindings/TraceWrapperReference.md).

[TOC]

## Header file

Unless otherwise noted, any of the primitives explained in this page requires the following `#include` statement:

```c++
#include "third_party/blink/renderer/platform/heap/handle.h"
```

## Base class templates

### GarbageCollected

A class that wants the lifetime management of its instances to be managed by Blink GC (Oilpan), it must inherit from
`GarbageCollected<YourClass>`.

```c++
class YourClass : public GarbageCollected<YourClass> {
    // ...
};
```

Instances of such classes are said to be *on Oilpan heap*, or *on heap* for short, while instances of other classes
are called *off heap*. In the rest of this document, the terms "on heap" or "on-heap objects" are used to mean the
objects on Oilpan heap instead of on normal (default) dynamic allocator's heap space.

You can create an instance of your class normally through `new`, while you may not free the object with `delete`,
as the Blink GC system is responsible for deallocating the object once it determines the object is unreachable.

You may not allocate an on-heap object on stack.

Your class may need to have a tracing method. See [Tracing](#Tracing) for details.

If your class needs finalization (i.e. some work needs to be done on destruction), use
[GarbageCollectedFinalized](#GarbageCollectedFinalized) instead.

`GarbageCollected<T>` or any class deriving from `GarbageCollected<T>`, directly or indirectly, must be the first
element in its base class list (called "leftmost derivation rule"). This rule is needed to assure each on-heap object
has its own canonical address.

```c++
class A : public GarbageCollected<A>, public P { // OK, GarbageCollected<A> is leftmost.
};

class B : public A, public Q { // OK, A is leftmost.
};

// class C : public R, public A { // BAD, A must be the first base class.
// };
```

If a non-leftmost base class needs to retain an on-heap object, that base class needs to inherit from
[GarbageCollectedMixin](#GarbageCollectedMixin). It's generally recommended to make *any* non-leftmost base class
inherit from `GarbageCollectedMixin`, because it's dangerous to save a pointer to a non-leftmost
non-`GarbageCollectedMixin` subclass of an on-heap object.

```c++
void someFunction(P*);

class A : public GarbageCollected<A>, public P {
public:
    void someMemberFunction()
    {
        someFunction(this); // DANGEROUS, a raw pointer to an on-heap object.
    }
};
```

### GarbageCollectedFinalized

If you want to make your class garbage-collected and the class needs finalization, your class needs to inherit from
`GarbageCollectedFinalized<YourClass>` instead of `GarbageCollected<YourClass>`.

A class is said to *need finalization* when it meets either of the following criteria:

*   It has non-empty destructor; or
*   It has a member that needs finalization.

```c++
class YourClass : public GarbageCollectedFinalized<YourClass> {
public:
    ~YourClass() { ... } // Non-empty destructor means finalization is needed.

private:
    scoped_refptr<Something> m_something; // scoped_refptr<> has non-empty destructor, so finalization is needed.
};
```

Note that finalization is done at an arbitrary time after the object becomes unreachable.

Any destructor executed within the finalization period must not touch any other on-heap object, because destructors
can be executed in any order. If there is a need of having such destructor, consider using
[EAGERLY_FINALIZE](#EAGERLY_FINALIZE).

Because `GarbageCollectedFinalized<T>` is a special case of `GarbageCollected<T>`, all the restrictions that apply
to `GarbageCollected<T>` classes also apply to `GarbageCollectedFinalized<T>`.

### GarbageCollectedMixin

A non-leftmost base class of a garbage-collected class may derive from `GarbageCollectedMixin`. If a direct child
class of `GarbageCollected<T>` or `GarbageCollectedFinalized<T>` has a non-leftmost base class deriving from
`GarbageCollectedMixin`, the garbage-collected class must declare the `USING_GARBAGE_COLLECTED_MIXIN(ClassName)` macro
in its class declaration.

A class deriving from `GarbageCollectedMixin` can be treated similarly as garbage-collected classes. Specifically, it
can have `Member<T>`s and `WeakMember<T>`s, and a tracing method. A pointer to such a class must be retained in the
same smart pointer wrappers as a pointer to a garbage-collected class, such as `Member<T>` or `Persistent<T>`.
The tracing method of a garbage-collected class, if any, must contain a delegating call for each mixin base class.

```c++
class P : public GarbageCollectedMixin {
 public:
  // OK, needs to trace q_.
  virtual void Trace(Visitor* visitor) { visitor->Trace(q_); }
 private:
  // OK, allowed to have Member<T>.
  Member<Q> q_;
};

class A : public GarbageCollected<A>, public P {
  USING_GARBAGE_COLLECTED_MIXIN(A);
 public:
  // Delegating call for P is needed.
  virtual void Trace(Visitor* visitor) { ...; P::Trace(visitor); }
  ...
};
```

Internally, `GarbageCollectedMixin` defines pure virtual functions, and `USING_GARBAGE_COLLECTED_MIXIN(ClassName)`
implements these virtual functions. Therefore, you cannot instantiate a class that is a descendant of
`GarbageCollectedMixin` but not a descendant of `GarbageCollected<T>`. Two or more base classes inheritng from
`GarbageCollectedMixin` can be resolved with a single `USING_GARBAGE_COLLECTED_MIXIN(ClassName)` declaration.

```c++
class P : public GarbageCollectedMixin { };
class Q : public GarbageCollectedMixin { };
class R : public Q { };

class A : public GarbageCollected<A>, public P, public R {
    USING_GARBAGE_COLLECTED_MIXIN(A); // OK, resolving pure virtual functions of P and R.
};

class B : public GarbageCollected<B>, public P {
    USING_GARBAGE_COLLECTED_MIXIN(B); // OK, different garbage-collected classes may inherit from the same mixin (P).
};

void someFunction()
{
    new A; // OK, A can be instantiated.
    // new R; // BAD, R has pure virtual functions.
}
```

## Class properties

### USING_GARBAGE_COLLECTED_MIXIN

`USING_GARBAGE_COLLECTED_MIXIN(ClassName)` is a macro that must be declared in a garbage-collected class, if any of
its base classes is a descendant of `GarbageCollectedMixin`.

See [GarbageCollectedMixin](#GarbageCollectedMixin) for the use of `GarbageCollectedMixin` and this macro.

### USING_PRE_FINALIZER

`USING_PRE_FINALIZER(ClassName, functionName)` in a class declaration declares the class has a *pre-finalizer* of name
`functionName`.

A pre-finalizer is a user-defined member function of a garbage-collected class that is called when the object is going
to be swept but before the garbage collector actually sweeps any objects. Therefore, it is allowed for a pre-finalizer
to touch any other on-heap objects, while a destructor is not. It is useful for doing some cleanups that cannot be done
with a destructor.

A pre-finalizer must have the following function signature: `void preFinalizer()`. You can change the function name.

```c++
class YourClass : public GarbageCollectedFinalized<YourClass> {
    USING_PRE_FINALIZER(YourClass, dispose);
public:
    void dispose()
    {
        m_other->dispose(); // OK; you can touch other on-heap objects in a pre-finalizer.
    }
    ~YourClass()
    {
        // m_other->dispose(); // BAD.
    }

private:
    Member<OtherClass> m_other;
};
```

Pre-finalizers have some implications on the garbage collector's performance: the garbage-collector needs to iterate
all registered pre-finalizers at every GC. Therefore, a pre-finalizer should be avoided unless it is really necessary.
Especially, avoid defining a pre-finalizer in a class that can be allocated a lot.

### EAGERLY_FINALIZE

A class-level annotation to indicate that class instances that the GC have determined as unreachable, should be eagerly
swept and finalized by the garbage collector, before the Blink thread (the mutator) resumes after a garbage
collection. The C++ destructor runs as part of this step. The default sweeping behavior is incremental, sweeping
pages as demanded by later heap allocations.

Like for the pre-finalizer mechanism, an `EAGERLY_FINALIZE()`d class is allowed to touch other heap objects, which
is sometimes required, but the main use case for eager finalization is to promptly let go of off-heap resources
and associations, by unregistering and destructing those eagerly. If not done, these external references would
otherwise attempt to unsafely access an effectively-dead object (pending lazy sweeping of its heap page.)

`EAGERLY_FINALIZE()` solves the same problem as pre-finalizers, but it arguably fits more naturally with the host
language's mechanism for finalization (C++ destructors.) One `EAGERLY_FINALIZE()` caveat is that the destructor
is not allowed to touch another eagerly finalized object (their finalization ordering isn't deterministic) nor
any pre-finalized objects. Choose the one you think best fits your need for prompt finalization.

### STACK_ALLOCATED

Class level annotation that should be used if the object is only stack allocated; it disallows use
of `operator new`. Any garbage-collected objects should be kept as `Member<T>` references, but you do not
need to define a `Trace()` method as they are on the stack, and automatically traced and kept alive should
a conservative GC be required.

Classes with this annotation do not need a `Trace()` method, and should not inherit a garbage collected class.

### DISALLOW_NEW()

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

On-stack references to on-heap objects must be raw pointers.

```c++
void someFunction()
{
    SomeGarbageCollectedClass* object = new SomeGarbageCollectedClass; // OK, retained by a pointer.
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
class SomeGarbageCollectedClass : public GarbageCollected<GarbageCollectedSomething> {
    ...
private:
    Member<AnotherGarbageCollectedClass> m_another; // OK, retained by Member<T>.
    WeakMember<AnotherGarbageCollectedClass> m_anotherWeak; // OK, weak reference.
};
```

The use of `WeakMember<T>` incurs some overhead in garbage collector's performance. Use it sparingly. Usually, weak
members are not necessary at all, because reference cycles with members are allowed.

More specifically, `WeakMember<T>` should be used only if the owner of a weak member can outlive the pointed object.
Otherwise, `Member<T>` should be used.

You need to trace every `Member<T>` and `WeakMember<T>` in your class. See [Tracing](#Tracing).

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
    Persistent<SomeGarbageCollectedClass> m_something; // OK, the object will be alive while this persistent is alive.
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

A garbage-collected class may need to have *a tracing method*, which lists up all the on-heap objects it has. The
tracing method is called when the garbage collector needs to determine (1) all the on-heap objects referred from a
live object, and (2) all the weak handles that may be filled with `nullptr` later. These are done in the "marking"
phase of the mark-and-sweep GC.

The basic form of tracing is illustrated below:

```c++
// In a header file:
class SomeGarbageCollectedClass
    : public GarbageCollected<SomeGarbageCollectedClass> {
 public:
  void Trace(Visitor*);

private:
  Member<AnotherGarbageCollectedClass> another_;
};

// In an implementation file:
void SomeGarbageCollectedClass::Trace(Visitor* visitor) {
  visitor->Trace(another_);
}
```

Specifically, if your class needs a tracing method, you need to declare and
define a `Trace(Visitor*)` method. This method is normally declared in the
header file and defined once in the implementation file, but there are
variations. Another common variation is to declare a virtual `Trace()` for base
classes that will be subclassed.

The function implementation must contain:

*   For each on-heap object `object` in your class, a tracing call: `visitor->Trace(object_);`.
*   If your class has one or more weak references (`WeakMember<T>`), you have the option of
    registering a *weak callback* for the object. See details below for how.
*   For each base class of your class `BaseClass` that is a descendant of `GarbageCollected<T>` or
    `GarbageCollectedMixin`, a delegation call to base class: `BaseClass::Trace(visitor);`"

It is recommended that the delegation call, if any, is put at the end of a tracing method.

The following example shows more involved usage:

```c++
class A : public GarbageCollected<A> {
 public:
  virtual void Trace(Visitor*) { } // Nothing to trace here.
};

class B : public A {
  // Nothing to trace here; exempted from having a tracing method.
};

class C : public B {
 public:
  void Trace(Visitor*) override;

 private:
  Member<X> x_;
  WeakMember<Y> y_;
  HeapVector<Member<Z>> z_;
};

void C::Trace(Visitor* visitor) {
    visitor->Trace(x_);
    visitor->Trace(y_); // Weak member needs to be traced.
    visitor->Trace(z_); // Heap collection does, too.
    B::Trace(visitor); // Delegate to the parent. In this case it's empty, but this is required.
}
```

Given that the class `C` above contained a `WeakMember<Y>` field, you could alternatively
register a *weak callback* in the trace method, and have it be invoked after the marking
phase:

```c++

void C::ClearWeakMembers(Visitor* visitor)
{
    if (ThreadHeap::isHeapObjectAlive(y_))
        return;

    // |m_y| is not referred to by anyone else, clear the weak
    // reference along with updating state / clearing any other
    // resources at the same time. None of those operations are
    // allowed to perform heap allocations:
    y_->detach();

    // Note: if the weak callback merely clears the weak reference,
    // it is much simpler to just |trace| the field rather than
    // install a custom weak callback.
    y_ = nullptr;
}

void C::Trace(Visitor* visitor) {
    visitor->template registerWeakMembers<C, &C::ClearWeakMembers>(this);
    visitor->Trace(x_);
    visitor->Trace(z_); // Heap collection does, too.
    B::Trace(visitor); // Delegate to the parent. In this case it's empty, but this is required.
}
```

Please notice that if the object (of type `C`) is also not reachable, its `Trace` method
will not be invoked and any follow-on weak processing will not be done. Hence, if the
object must always perform some operation when the weak reference is cleared, that
needs to (also) happen during finalization.

Weak callbacks have so far seen little use in Blink, but a mechanism that's available.

## Heap collections

Heap collections are WTF collection types that support `Member<T>`, `WeakMember<T>`(see [below](#Weak collections)), and garbage collected objects as its elements.

Here is the complete list:

- WTF::Vector → blink::HeapVector
- WTF::Deque → blink::HeapDeque
- WTF::HashMap → blink::HeapHashMap
- WTF::HashSet → blink::HeapHashSet
- WTF::LinkedHashSet → blink::HeapLinkedHashSet
- WTF::ListHashSet → blink::HeapListHashSet
- WTF::HashCountedSet → blink::HeapHashCountedSet

These heap collections work mostly the same way as their WTF collection counterparts but there are some things to keep in mind.

Heap collections are special in that the types themselves do not inherit from GarbageCollected (hence they are not allocated on the Oilpan heap) but they still *need to be traced* from the trace method (because we need to trace the backing store which is on the Oilpan heap).

```c++
class MyGarbageCollectedClass : public GarbageCollected<MyGarbageCollectedClass> {
 public:
  void Trace(Visitor* visitor) { visitor->Trace(list_); }
 private:
  HeapVector<Member<AnotherGarbageCollectedClass>> list_;
};
```

When you want to add a heap collection as a member of a non-garbage-collected class (on the main thread), please use a Persistent to reference it.

```c++
class MyNotGarbageCollectedClass {
 private:
  Persistent<HeapVector<Member<MyGarbageCollectedClass>>> list_;
};
```

On non-main threads these persistent heap collections have been disabled to simplify the thread termination sequence. Please wrap the heap collections in a `Persistent` instead.

Please be very cautious if you want to use a heap collection from multiple threads. Reference to heap collections may be passed to another thread using CrossThreadPersistents, but *you may not modify the collection from the non-owner thread*. This is because modifications to collections may trigger backing store reallocations, and Oilpan's per thread heap requires that modifications to a heap happen on its owner thread.

### Weak collections

You can put `WeakMember<T>` in heap collections except for `HeapVector` and `HeapDeque` which we do not support.

During an Oilpan GC, the weak members that refernce a collected object will be removed from its heap collection, meaning the size of the collection will shrink and you do not have to check for null weak members when iterating through the collection.

## Traits helpers

At times, one may be working on code that needs to deal with both "regular" types and classes managed by the Blink GC. The following helpers can aid in writing code that needs to use different wrappers and containers based on whether a type is managed by Oilpan.

### AddMemberIfNeeded<T>

Given a type `T`, defines a type alias that is either `Member<T>` or `T` depending on whether `T` is a type managed by the Blink GC.

```c++
class MyGarbageCollectedClass : public GarbageCollected<MyGarbageCollectedClass> {
  // ...
};

class MyNotGarbageCollectedClass {
  // ...
};

AddMemberIfNeeded<MyNotGarbageCollectedClass> v1;  // MyNotGarbageCollectedClass v1;
AddMemberIfNeeded<int32_t> v2;                     // int32_t v2;
AddMemberIfNeeded<MyGarbageCollectedClass> v3;     // Member<MyGarbageCollectedClass> v3;
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
  void Trace(Visitor* visitor) {
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
  void Trace(Visitor* visitor) {
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
