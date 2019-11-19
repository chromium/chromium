# Integer Semantics, Unsafety, And You

[TOC]

These handy tips apply in any memory management situation and in any kind of IPC
situation (classic Chromium IPC, Mojo, Windows/POSIX IPC, Mach IPC, files,
sockets, parsing binary formats, ...).

Basically, don't believe the lie that 'computers are good at arithmetic'. In
general, unless you explicitly check an arithmetic operation, it's safest to
assume the operation went wrong. The least painful way to systematically check
arithmetic is Chromium's base/numerics templates and helper functions.

## Be Aware Of The Subtleties Of Integer Types

First [read about the scary security implications of integer arithmetic in
C/C++](http://en.wikipedia.org/wiki/Integer_overflow). Adhere to these best
practices:

* Use the [integer templates and cast templates in
base/numerics](../../base/numerics/README.md) to avoid overflows, **especially when
calculating the size or offset of memory allocations**.
* Use unsigned types for values that shouldn't be negative or where defined
overflow behavior is required. (Overflow is undefined behavior for signed
types!)
* Across any process boundary, use explicitly sized integer types, such as
`int32_t`, `int64_t`, or `uint32_t`, since caller and callee could potentially
use different interpretations of implicitly-sized types like `int` or `long`.
(For example, a 64-bit browser process and a 32-bit plug-in process might
interpret `long` differently.)

## Be Aware Of The Subtleties Of Integer Types Across Languages

### Java

When writing code for Chromium on Android, you will often need to marshall
arrays, and their sizes and indices, across the language barrier (and possibly
also across the IPC barrier). The trouble here is that the Java integer types
are well-defined, but the C++ integer types are whimsical. A Java `int` is a
signed 32-bit integer with well-defined overflow semantics, and a Java `long` is
a signed 64-bit integer with well-defined overflow semantics. in C++, only the
explicitly-sized types (e.g. `int32_t`) have guaranteed exact sizes, and only
unsigned integers (of any size) have defined overflow semantics.

Essentially, Java integers **actually are** what people often (incorrectly)
**assume** C++ integers are. Furthermore, Java `Array`s are indexed with Java
`int`s, whereas C++ arrays are indexed with `size_t` (often implicitly cast, of
course). Note that this also implies a 2^31 limit on the number of elements in
an array that is coming from or going to Java. That Should Be Enough For
Anybody, but it's good to keep in mind.

You need to make sure that every integer value survives its journey across
languages intact. That generally means explicit casts with range checks; the
easiest way to do this is with the `base::checked_cast` or (much less likely)
`base::saturated_cast` templates in base/numerics. Depending on how the integer
object is going to be used, and in which direction the value is flowing, it may
make sense to cast the value to `jint` (an ID or regular integer), `jlong` (a
regular long integer), `size_t` (a size or index), or one of the other more
exotic C/C++ integer types like `off_t`.

### JavaScript And JSON

[Here is some good reading on integers in
JavaScript](http://2ality.com/2014/02/javascript-integers.html). TL;DR:

* Normal JavaScript `Number`s have a 'safe' integer range of 53 bits (signed).
See `Number.isSafeInteger`, `Number.MIN_SAFE_INTEGER`, and
`Number.MAX_SAFE_INTEGER`.
* Array indices are unsigned 32-bit values.
* Character codes (`fromCharCode`, `charCodeAt`) are unsigned 16-bit values.
