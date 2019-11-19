# Strings in Blink

_Everything you always wanted to know but were afraid to ask_

This document covers the `String` type in Blink, often written with an
explicit namespace as `WTF::String` to disambiguate from string
concepts or other types. It also briefly covers associated classes
used for constructing strings (`StringBuilder`, `StringBuffer`), the
internal `StringImpl` class, and the special `AtomicString` variant.
It does not cover other text-related types or utilities (e.g.
encodings, views, line endings, etc).

## Overview

A `WTF::String` represents a sequence of zero or more Unicode code
points. A `String` can also represent one of two zero-length strings:
the empty string and the null string. These correspond to "" and
`null` in JavaScript, respectively. Both the empty and the null string
return true from `String::IsEmpty()` but only the null string returns
true from `String::IsNull()`.


Unlike `std::string`, Blink’s `String` object is a pointer to a
reference counted character buffer. This design makes it easier to
share the underlying character buffer between different consumers
because multiple consumers can reference the same underlying buffer.
The disadvantage of this design is that we need to be careful when
mixing Strings with multithreading because the character buffer’s
reference counting is not thread safe.


## Storage

### Encoding

A `String` can represent Unicode code points with either `LChar`s or
`UChar`s, which use 8 bits and 16 bits per code unit respectively.
Each `LChar` represents a single code unit of ISO-8859-1, more
commonly called Latin-1 (hence the L in `LChar`). Unlike UTF-8, this
encoding cannot represent every Unicode code point. However, also
unlike UTF-8, every representable Unicode code point can represented
with a single code unit and the code unit is simply the 8 least
significant bits of the code point. This property makes Latin-1 an
attractive encoding because we can decode a Latin-1 code unit to a
Unicode code point simply by zero-extending the `LChar` value.


Each `UChar` represents a single UTF-16 code unit (hence the U in
`UChar`). Unfortunately, Strings do not always contain valid UTF-16
sequences. Strings that have round-tripped through JavaScript can
contain invalid UTF-16 sequences because JavaScript isn’t required to
pair surrogates in its strings. Most code that works with Strings can
ignore this issue because they operate code-unit-by-code-unit, but
subsystems need to operate on code points outside the Basic
Multilingual Plane need to be prepared to handle unpaired surrogates.


In addition to `LChar` and `UChar`, Strings also use the type
`UChar32`, which is a UTF-32 code unit. `UChar32` is particularly easy
to work with because every UTF-32 code unit has the same numerical
value as its corresponding Unicode code point. Practically speaking,
that means you can treat `UChar32` values as if they were Unicode code
points.

### Layout

The `String` object itself is simply a pointer to a `StringImpl`
object, which contains the actual character buffer. `String` uses a
`scoped_refptr<>` to automatically `AddRef()` and `Release()` the
`StringImpl` object on construction and destruction. The `StringImpl`
pointer can be zero, in which case the `String` represents the null
string. Typically, `String` objects are allocated on the stack or as
members variables. `StringImpl` objects are always allocated in the
heap.


`StringImpl` objects are (logically) immutable, but a given `String`
object can refer to different `StringImpl` objects over time. For
example, `String::Replace()` works by creating a new `StringImpl`
object with the replaced characters rather than by mutating the
original `StringImpl` object.


Rather than using a fixed `sizeof(StringImpl)` allocation size,
`StringImpl` object are allocated a variable amount of memory and
store their character data in the same memory allocation as the
`StringImpl` object itself. In a sense, the `StringImpl` object is a
header for the actual array of characters, be they `LChar`s or
`UChar`s.

#### Reference count

The `StringImpl` header contains three 32 bit fields. The first is a
reference count, which is incremented and decremented by the
`AddRef()` and `Release()` functions. When the reference count reaches
zero the `StringImpl` object is deallocated, unless the `StringImpl`
is marked static (in which case the `StringImpl` object is never
deallocated).


#### Length

The next 32 bits represent the (potentially zero) length of the
string. The length field always represents the number of code units,
regardless of whether the `StringImpl` uses `LChar`s or `UChar`s. We
use a 32 bit length on both 32-bit and 64-bit systems. To avoid
creating a string whose length is too long to represent in 32 bits, we
RELEASE_ASSERT that the length doesn’t overflow, which means we’ll
crash in a controlled way if you try to create a string that’s
absurdly long.

#### Hash and flags

The final 32 bits are used to cache the string’s hash value and to
store a number of Boolean flags. We use 24 bits for the hash and
reserve 8 bits for flags. As of April 2019, the flags represent
whether the `StringImpl`:

* ...contains only ASCII (7-bit), or whether this is unknown (2
    flags).
* ...is a member of the `AtomicString` table (discussed below).
* ...contains `LChar`s or `UChar`s.
* ...is Static and will never be deallocated (discussed below).

We haven’t evaluated the performance trade-offs of caching the hash
value in the `StringImpl` object recently. It’s possible that the
cache hit rate is sufficiently low that we should remove the final 32
bit field and move the two flags into the length, reducing the length
field to 30 bits. Small changes to `StringImpl` can have large effects
on the overall system, which means we should measure the performance
impact of this sort of change carefully.


## Construction

There are multiple interfaces for constructing strings, each of which
is useful in different situations.

### `String` constructor
The most straightforward interface for constructing strings is the
`String` constructor. You should use this interface when the source
data for the `String` is an array of `LChar`s or `UChar`s. The
`String` constructor allocates a new `StringImpl` object and copies
the source characters into the `StringImpl` object as efficiently as
possible.


The `String` constructor that takes a `UChar` array always creates a
`UChar`-based `StringImpl` object even if all the source characters
would actually fit in `LChar`s. If your source data is an array of
`UChar`s but you have reason to believe that the string will usually
be representable in Latin-1, you should consider using
`StringImpl::Create8BitIfPossible()`, which creates an `LChar`- or
`UChar`-based `StringImpl` object as appropriate at the cost of
checking whether all the source characters can be represented in
Latin-1.


### `operator+`

The + operator on Strings is the most efficient way to combine smaller
strings into larger strings. Using templates, `operator+` builds a
tree of temporary objects that mirrors the tree of `operator+`
invocations. When the temporary objects are (implicitly) collapsed to
a `String`, we first compute the length of the final string and then
allocate a single `StringImpl` object of exactly the correct size.
After allocating the object, we copy all the characters into the
string. This approach means we copy the characters exactly once into
the correctly sized buffer, which is maximally efficient.

### `StringBuilder`

If you’re unable to use `operator+` to build your string, for example
because you need to use a loop, you should use `StringBuilder`. Like
similar interfaces in other libraries, `StringBuilder` lets you build
a `String` by incrementally appending content. `StringBuilder` tries
to use 8-bit `StringImpl` objects whenever possible but will upconvert
its internal buffer to 16 bits if necessarily.


`StringBuilder::Append()` grows its buffer exponentially, which means
`StringBuilder` avoids the pathologically bad O(N^2) performance that
repeated appends/concatenation can cause. One way to further optimize
`String` construction when using `StringBuilder` is to call
`StringBuilder::ReserveCapacity()` with (an estimate of) the final
length of the `String` (in code units) before appending characters. If
you give `StringBuilder` an accurate estimate of the length of the
string, `StringBuilder` can pre-allocate the appropriate amount of
memory and avoid having to reallocate its buffer and copy your string.


### `StringBuffer`

Finally, there are some cases where neither the `String` constructor
or `StringBuilder` work well. For example, sometimes rather than
having a source array of `UChar`s from which to construct a String,
you might have a function that will write the `UChar`s into a buffer
you provide. `StringBuffer` can help you in these cases by allocating
a character buffer and letting the function write into it.


Conceptually, a `StringBuffer` represents the underlying character
buffer from a String. However, unlike `StringImpl` objects,
`StringBuffer`s are mutable. `StringBuffer`s work well when you know
ahead of time exactly how large a buffer you need and whether you want
to use `LChar`s or `UChar`s. If you’re uncertain about the length of
the `String` you’re constructing, you probably should use
`StringBuilder`.

## Atomic Strings

Some `StringImpl` objects are marked as _Atomic_, which means they’re
stored in a thread-local `HashSet` called the `AtomicString` table.
Rather than interacting directly with these anointed `StringImpl`
objects, we usually hold pointers to them via `AtomicString` objects
(rather than `String` objects). Using `AtomicString` rather than
`String` to point to an Atomic `StringImpl` object lets the compiler
generate (and skip!) the appropriate hash lookups in the
`AtomicString` table as well as use faster comparison operations with
other `AtomicString`s.

### Construction

Typically, constructing an `AtomicString` from a `String` object will
involve a hash lookup in the `AtomicString` table for the current
thread. If the string represented by the `String` object is not
present in the `AtomicString` table, the `StringImpl` object from that
`String` will be marked Atomic and added to the table. If the
represented string is already present in the `AtomicString` table, the
already-Atomic `StringImpl` object from the table will be used to
construct the `AtomicString` rather than the `StringImpl` from the
original String.


If you wish to construct an `AtomicString` from an array of `LChar`s
or `UChar`s, you should use the `AtomicString` constructor directly
rather than first constructing a `String` object. If the string is
already present in the `AtomicString` table, the `AtomicString`
constructor will grab a reference to the existing Atomic `StringImpl`
object rather than first allocating and populating a `StringImpl`
object as the `String` constructor would.

### Fast comparisons

If two `StringImpl` objects are atomic, you can compare them for
equality by comparing their addresses rather than by comparing them
character-by-character. The reason this works is that we maintain the
invariant that no two Atomic `StringImpl` objects on a given thread
represent the same string. Therefore, the two `StringImpl` objects
represent the same string if, and only if, they are actually the same
`StringImpl` object. We’ve overloaded `operator==` on `AtomicString`
to let the compiler generate these optimized comparisons
automatically.

### Deduplication

Because there are no duplicate Atomic `StringImpl` objects on a given
thread, `AtomicString`s are useful for coalescing duplicate strings
into a single `StringImpl` object, saving memory. Unfortunately,
`AtomicString`s are thread-specific and cannot be used to coalesce
duplicate strings across threads.

## Threading

### Isolated copies

String objects that you construct yourself are not thread-safe. The
underlying `StringImpl` object can be shared only by `String` objects
on the same thread because the `StringImpl`’s reference count isn’t
incremented or decremented atomically.


In some limited cases, you can safely send a `String` from one thread
to another. In order for that to be safe, you need to make sure that
the underlying `StringImpl` object has exactly one reference---the one
you’re sending to another thread. If there is only one outstanding
reference to the `StringImpl`, then there won’t be any reference count
data races. The easiest way to get a `StringImpl` object with only one
reference is to call `String::IsolatedCopy()`. You can check that a
given `String` is safe to send to another thread by calling
`String::IsSafeToSendToAnotherThread()`, typically in a `DCHECK`.


If you look at the implementation of `IsSafeToSendToAnotherThread()`,
you’ll notice that it always returns false if the `StringImpl` is
Atomic, regardless of the reference count. That’s because Atomic
`StringImpl` objects are not safe to send to another thread because
they’re associated with an `AtomicString` table local to the current
thread. If you do send an `AtomicString` to another thread and the
`StringImpl` object is destructed on that thread, it will try to
remove itself from that thread’s `AtomicString` table rather than from
the original thread’s `AtomicString` table.

### Static Strings

At startup, we create a number of _Static_ `StringImpl` objects that
are safe to use from any thread. These `StringImpl` objects maintain
the invariant that the least significant bit of their reference count
is always one, which means their reference count never reaches zero
and they are never deallocated. In addition to preventing their
deallocation, we also pre-populate the hash value to ensure that
Static `StringImpl` objects are otherwise immutable.


We first introduced these Static strings for the threaded HTML parser,
but we are gradually using them more widely in the codebase. There are
still some delicate interactions between Static strings and the
`AtomicString` table, but hopefully we’ll smooth over these rough
edges over time.

# Conclusion

This document contains a brief introduction to Blink’s `String` class.
There are many details that are not included, but hopefully this
document has given you a good high-level understanding of Strings.
More details are available in the source, either in code or in
comments. Happy hacking!

_Originally authored by Adam Barth (abarth), 5 August 2013._
