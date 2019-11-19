
# Blink GC Design

Oilpan is a garbage collection system for Blink objects.
This document explains the design of the GC.
If you're just interested in how to use Oilpan,
see [BlinkGCAPIReference](BlinkGCAPIReference.md).

[TOC]

## Overview

Oilpan implements a mark-and-sweep GC. It features thread-local garbage
collection with incremental marking and lazy sweeping. It can also do
compaction for a subset of objects (collection backings).

## Threading model

Oilpan creates a different heap and root set for each thread. This allows Oilpan
to run garbage collection in parallel with mutators running in other threads.

Any object or `Persistent` that is allocated on a thread automatically belong to
that thread's heap or root set. References to objects belonging to another
thread's heap, must use the `CrossThreadPersistent` handle. This is even true
for on-heap to on-heap references.

Assigning to a `CrossThreadPersistent` requires a global lock, meaning it might
block waiting for garbage collection to end on all other threads.

Threads that want to allocate Oilpan objects must be "attached" to Oilpan
(typically through `WebThreadSupportingGC`).

## Heap partitioning

As mentioned earlier, we have separate heaps for each thread. This `ThreadHeap`
is further partitioned into "Arenas". The Arena for an object is chosen
depending on a number of criteria.

For example
- objects over 64KiB goes into `kLargeObjectArenaIndex`
- objects that is a collection backing goes into one of the collection backing
arenas
- objects that is a Node or a CSSValue goes into one of the typed arenas
- other objects goes into one of the normal page arenas bucketed depending on
their size

## Precise GC and conservative GC

Oilpan has three kinds of GCs.

Precise GC is triggered at the end of an event loop. At this point, it is
guaranteed that there are no on-stack pointers pointing to Oilpan's heap. Oilpan
can just trace from the `Persistent` handles and collect all garbage precisely.

Conservative GC runs when we are under memory pressure, and a GC cannot wait
until we go back to an event loop. In this case, the GC scans the native stack
and treats the pointers discovered via the native stacks as part of the root
set. (That's why raw pointers are used instead of handles on the native stack.)

Incremental GC is the most common type of GC. It splits the marking phase into
small chunks and runs them between tasks. The smaller pause times help with
reducing jank.

## Marking phase

The marking phase consists of the following steps. The marking phase is executed
in a stop-the-world manner.

Step 1. Mark all objects reachable from the root set by calling `Trace()`
methods defined on each object.

Step 2. Clear out all weak handles and run weak callbacks.

To prevent a use-after-free from happening, it is very important to
make sure that Oilpan doesn't mis-trace any edge of the object graph.
This means that all pointers except on-stack pointers must be wrapped
with Oilpan's handles (i.e., Persistent<>, Member<>, WeakMember<> etc).
Raw pointers to on-heap objects have a risk of creating an edge Oilpan
cannot understand and causing a use-after-free. Raw pointers shall not be used
to reference on-heap objects (except raw pointers on native stacks). Exceptions
can be made if the target object is guaranteed to be kept alive in other ways.

## Sweeping phase

The sweeping phase consists of the following steps.

Step 1. Invoke pre-finalizers.
At this point, no destructors have been invoked.
Thus the pre-finalizers are allowed to touch any other on-heap objects
(which may get destructed in this sweeping phase).

Step 2. The thread resumes mutator's execution. (A mutator means user code.)

Step 3. As the mutator allocates new objects, lazy sweeping invokes
destructors of the remaining dead objects incrementally.

There is no guarantee of the order in which the destructors are invoked.
That's why destructors must not touch any other on-heap objects
(which might have already been destructed). If some destructor unavoidably
needs to touch other on-heap objects, it will have to be converted to a
pre-finalizer. The pre-finalizer is allowed to touch other on-heap objects.

The mutator is resumed before all the destructors has run.
For example, imagine a case where X is a client of Y, and Y holds
a list of clients. If the code relies on X's destructor removing X from the list,
there is a risk that Y iterates the list and calls some method of X
which may touch other on-heap objects. This causes a use-after-free.
Care must be taken to make sure that X is explicitly removed from the list
before the mutator resumes its execution in a way that doesn't rely on
X's destructor.

Either way, the most important thing is that there is no guarantee of
when destructors run. Assumptions should not be made about the order and the
timing of their execution.
(In general, it's dangerous to do something complicated in a destructor.)

Notes (The followings are features that shall be reserved for unusual
destruction requirements):

* Weak processing runs only when the holder object of the WeakMember
outlives the pointed object. If the holder object and the pointed object die
at the same time, the weak processing doesn't run. It is wrong to write code
assuming that the weak processing always runs.

* Pre-finalizers are heavy because the thread needs to scan all pre-finalizers
at each sweeping phase to determine which pre-finalizers to be invoked
(the thread needs to invoke pre-finalizers of dead objects). Adding
pre-finalizers to frequently created objects should be avoided.
