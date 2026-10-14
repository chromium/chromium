# Modern C++ use in Chromium

_This document is part of the more general
[Chromium C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md).
It summarizes the supported state of new and updated language and library
features in recent C++ standards and the [Abseil](https://abseil.io/about/)
library. This guide applies to both Chromium and its subprojects, though
subprojects can choose to be more restrictive if necessary for toolchain
support._

The C++ language has in recent years received an updated standard every three
years (C++11, C++14, etc.).
[For various reasons](https://chromium.googlesource.com/chromium/src/+/main/docs/process/c++_version_upgrades.md),
Chromium does not immediately allow new features on the publication of such a
standard. Instead, once Chromium supports the toolchain to a certain extent
(e.g., build support is ready), a standard is declared "_initially supported_",
with new language/library features banned pending discussion but not yet
allowed.

Unless explicitly listed as banned or TBD below, member functions added in a
new C++ standard for already allowed standard library types are implicitly
allowed.

You can propose changing the status of a feature by sending an email to
[cxx@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/cxx).
Include a short blurb on what the feature is and why you think it should or
should not be allowed, along with links to any relevant previous discussion. If
the list arrives at some consensus, send a codereview to change this file
accordingly, linking to your discussion thread.

If an item remains on the TBD list two years after initial support is added,
style arbiters should explicitly move it to an appropriate allowlist or
blocklist, allowing it if there are no obvious reasons to ban.

The current status of existing standards and Abseil features is:

*   **C++11:** _Default allowed; see banned features below_
*   **C++14:** _Default allowed_
*   **C++17:** _Default allowed; see banned features below_
*   **C++20:** _Default allowed; see banned features below_
*   **C++23:** _Initially supported January 2026; see allowed/banned/TBD
    features below_
*   **C++26:** _Not yet supported_
*   **Abseil:** _Default allowed; see banned/TBD features below. The following
    dates represent the start of the two-year TBD periods for certain parts of
    Abseil:_
      * absl::linked_hash_set & map: Initially added to third_party Dec 30, 2025
      * absl::optional_ref: Initially added to third_party Feb 25, 2026
      * absl::SourceLocation: Initially added to third_party Mar 11, 2026
      * absl::AnySpan: Initially added to third_party Apr 7, 2026

## Banned features and third-party code

Third-party libraries may generally use banned features internally, although features
with poor compiler support or poor security properties may make the library
unsuitable to use with Chromium.

Chromium code that calls functions exported from a third-party library may use
banned library types that are required by the interface, as long as:

 * The disallowed type is used only at the interface, and converted to and from
   an equivalent allowed type as soon as practical on the Chromium side.
 * The feature is not banned due to security issues or lack of compiler support.
   If it is, discuss with
   [cxx@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/cxx)
   to find a workaround.

[TOC]

## C++11 Banned Language Features {#core-blocklist-11}

The following C++11 language features are not allowed in the Chromium codebase.

### Inline Namespaces <sup>[banned]</sup>

```c++
inline namespace foo { ... }
```

**Description:** Allows better versioning of namespaces.

**Documentation:**
[Inline namespaces](https://en.cppreference.com/w/cpp/language/namespace#Inline_namespaces)

**Notes:**
*** promo
Banned in the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#Namespaces).
Unclear how it will work with components.
***

### long long Type <sup>[banned]</sup>

```c++
long long var = value;
```

**Description:** An integer of at least 64 bits.

**Documentation:**
[Fundamental types](https://en.cppreference.com/w/cpp/language/types)

**Notes:**
*** promo
Use a `<stdint.h>` type if you need a 64-bit number.

[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/RxugZ-pIDxk)
***

### User-Defined Literals <sup>[banned]</sup>

```c++
DistanceType var = 12_km;
```

**Description:** Allows user-defined literal expressions.

**Documentation:**
[User-defined literals](https://en.cppreference.com/w/cpp/language/user_literal)

**Notes:**
*** promo
Banned in the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#Operator_Overloading).
***

## C++11 Banned Library Features {#library-blocklist-11}

The following C++11 library features are not allowed in the Chromium codebase.

### &lt;cctype&gt;, &lt;ctype.h&gt;, &lt;cwctype&gt;, &lt;wctype.h&gt; <sup>[banned]</sup>

```c++
#include <cctype>
#include <cwctype>
#include <ctype.h>
#include <wctype.h>
```

**Description:** Provides utilities for ASCII characters.

**Documentation:**
[Standard library header `<cctype>`](https://en.cppreference.com/w/cpp/header/cctype),
[Standard library header `<cwctype>`](https://en.cppreference.com/w/cpp/header/cwctype)

**Notes:**
*** promo
Banned due to dependence on the C locale as well as UB when arguments don't fit
in an `unsigned char`/`wchar_t`. Use similarly-named replacements in
[third_party/abseil-cpp/absl/strings/ascii.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/strings/ascii.h)
instead.
***

### &lt;cfenv&gt;, &lt;fenv.h&gt; <sup>[banned]</sup>

```c++
#include <cfenv>
#include <fenv.h>
```

**Description:** Provides floating point status flags and control modes for
C-compatible code.

**Documentation:**
[Standard library header `<cfenv>`](https://en.cppreference.com/w/cpp/header/cfenv)

**Notes:**
*** promo
Banned by the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#C++11)
due to concerns about compiler support.
***

### &lt;chrono&gt; <sup>[banned]</sup>

```c++
#include <chrono>
```

**Description:** A standard date and time library.

**Documentation:**
[Date and time utilities](https://en.cppreference.com/w/cpp/chrono)

**Notes:**
*** promo
Overlaps with `base/time`.
***

### &lt;exception&gt; <sup>[banned]</sup>

```c++
#include <exception>
```

**Description:** Exception throwing and handling.

**Documentation:**
[Standard library header `<exception>`](https://en.cppreference.com/w/cpp/header/exception)

**Notes:**
*** promo
Exceptions are banned by the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#Exceptions)
and disabled in Chromium compiles. However, the `noexcept` specifier is
explicitly allowed.

[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/8i4tMqNpHhg)
***

### Engines And Generators From &lt;random&gt; <sup>[banned]</sup>

```c++
std::mt19937 generator;
```

**Description:** Methods of generating random numbers.

**Documentation:**
[Pseudo-random number generation](https://en.cppreference.com/w/cpp/numeric/random)

**Notes:**
*** promo
Do not use any random number engines or generators from `<random>`. Instead, use
`base::RandomBitGenerator`. (You may use the distributions from `<random>`.)

[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/16Xmw05C-Y0)
***

### &lt;ratio&gt; <sup>[banned]</sup>

```c++
#include <ratio>
```

**Description:** Provides compile-time rational numbers.

**Documentation:**
[`std::ratio`](https://en.cppreference.com/w/cpp/numeric/ratio/ratio)

**Notes:**
*** promo
Banned by the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#C++11)
due to concerns that this is tied to a more template-heavy interface style.
***

### &lt;regex&gt; <sup>[banned]</sup>

```c++
#include <regex>
```

**Description:** A standard regular expressions library.

**Documentation:**
[Regular expressions library](https://en.cppreference.com/w/cpp/regex)

**Notes:**
*** promo
Overlaps with many regular expression libraries in Chromium. When in doubt, use
`third_party/re2`.
***

### std::aligned_{storage,union} <sup>[banned]</sup>

```c++
std::aligned_storage<sizeof(T), alignof<T>>::type buf;
```

**Description:** Creates aligned, uninitialized storage to later hold one or
more objects.

**Documentation:**
[`std::aligned_storage`](https://en.cppreference.com/w/cpp/types/aligned_storage)

**Notes:**
*** promo
Deprecated in C++23. Generally, use `alignas(T) char buf[sizeof(T)];`. Aligned
unions can be handled similarly, using the max alignment and size of the union
members, either passed via a pack or computed inline.
***

### std::bind <sup>[banned]</sup>

```c++
auto x = std::bind(function, args, ...);
```

**Description:** Declares a function object bound to certain arguments.

**Documentation:**
[`std::bind`](https://en.cppreference.com/w/cpp/utility/functional/bind)

**Notes:**
*** promo
Use `base::Bind` instead. Compared to `std::bind`, `base::Bind` helps prevent
lifetime issues by preventing binding of capturing lambdas and by forcing
callers to declare raw pointers as `Unretained`.

[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/SoEj7oIDNuA)
***

### std::function <sup>[banned]</sup>

```c++
std::function x = [] { return 10; };
std::function y = std::bind(foo, args);
```

**Description:** Wraps a standard polymorphic function.

**Documentation:**
[`std::function`](https://en.cppreference.com/w/cpp/utility/functional/function)

**Notes:**
*** promo
Use `base::{Once,Repeating}Callback` or `base::FunctionRef` instead. Compared
to `std::function`, `base::{Once,Repeating}Callback` directly supports
Chromium's refcounting classes and weak pointers and deals with additional
thread safety concerns.

[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/SoEj7oIDNuA)
***

### std::shared_ptr <sup>[banned]</sup>

```c++
std::shared_ptr<int> x = std::make_shared<int>(10);
```

**Description:** Allows shared ownership of a pointer through reference counts.

**Documentation:**
[`std::shared_ptr`](https://en.cppreference.com/w/cpp/memory/shared_ptr)

**Notes:**
*** promo
Unlike `base::RefCounted`, uses extrinsic rather than intrinsic reference
counting. Could plausibly be used in Chromium, but would require significant
migration.

[Google Style Guide](https://google.github.io/styleguide/cppguide.html#Ownership_and_Smart_Pointers),
[Discussion thread](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/aT2wsBLKvzI)
***

### std::{sto{i,l,ul,ll,ull,f,d,ld},to_string} <sup>[banned]</sup>

```c++
int x = std::stoi("10");
```

**Description:** Converts strings to/from numbers.

**Documentation:**
[`std::stoi`, `std::stol`, `std::stoll`](https://en.cppreference.com/w/cpp/string/basic_string/stol),
[`std::stoul`, `std::stoull`](https://en.cppreference.com/w/cpp/string/basic_string/stoul),
[`std::stof`, `std::stod`, `std::stold`](https://en.cppreference.com/w/cpp/string/basic_string/stof),
[`std::to_string`](https://en.cppreference.com/w/cpp/string/basic_string/to_string)

**Notes:**
*** promo
The string-to-number conversions rely on exceptions to communicate failure,
while the number-to-string conversions have performance concerns and depend on
the locale. Use `base/strings/string_number_conversions.h` instead.
***

### std::weak_ptr <sup>[banned]</sup>

```c++
std::weak_ptr<int> x = my_shared_x;
```

**Description:** Allows a weak reference to a `std::shared_ptr`.

**Documentation:**
[`std::weak_ptr`](https://en.cppreference.com/w/cpp/memory/weak_ptr)

**Notes:**
*** promo
Banned because `std::shared_ptr` is banned.  Use `base::WeakPtr` instead.
***

### Thread Support Library <sup>[banned]</sup>

```c++
#include <barrier>             // C++20
#include <condition_variable>
#include <future>
#include <latch>               // C++20
#include <mutex>
#include <semaphore>           // C++20
#include <stop_token>          // C++20
#include <thread>
```

**Description:** Provides a standard multithreading library using `std::thread`
and associates

**Documentation:**
[Thread support library](https://en.cppreference.com/w/cpp/thread)

**Notes:**
*** promo
Overlaps with `base/synchronization`. `base::Thread` is tightly coupled to
`base::MessageLoop` which would make it hard to replace. We should investigate
using standard mutexes, or `std::unique_lock`, etc. to replace our
locking/synchronization classes.
***

## C++17 Banned Language Features {#core-blocklist-17}

The following C++17 language features are not allowed in the Chromium codebase.

### UTF-8 character literals <sup>[banned]</sup>

```c++
char x = u8'x';     // C++17
char8_t x = u8'x';  // C++20
```

**Description:** A character literal that begins with `u8` is a character
literal of type `char` (C++17) or `char8_t` (C++20). The value of a UTF-8
character literal is equal to its ISO 10646 code point value.

**Documentation:**
[Character literal](https://en.cppreference.com/w/cpp/language/character_literal)

**Notes:**
*** promo
Banned because `char8_t` is banned. Use an unprefixed character or string
literal; it should be encoded in the binary as UTF-8 on all supported platforms.
***

## C++17 Banned Library Features {#library-blocklist-17}

The following C++17 library features are not allowed in the Chromium codebase.

### Mathematical special functions <sup>[banned]</sup>

```c++
std::assoc_laguerre()
std::assoc_legendre()
std::beta()
std::comp_ellint_1()
std::comp_ellint_2()
std::comp_ellint_3()
std::cyl_bessel_i()
std::cyl_bessel_j()
std::cyl_bessel_k()
std::cyl_neumann()
std::ellint_1()
std::ellint_2()
std::ellint_3()
std::expint()
std::hermite()
std::legendre()
std::laguerre()
std::riemann_zeta()
std::sph_bessel()
std::sph_legendre()
std::sph_neumann()
```

**Description:** A variety of mathematical functions.

**Documentation:**
[Mathematical special functions](https://en.cppreference.com/w/cpp/numeric/special_functions)

**Notes:**
*** promo
Banned due to
[lack of libc++ support](https://libcxx.llvm.org/Status/Cxx17.html).
***

### Parallel algorithms <sup>[banned]</sup>

```c++
auto it = std::find(std::execution::par, std::begin(vec), std::end(vec), 2);
```

**Description:** Many of the STL algorithms, such as the `copy`, `find` and
`sort` methods, now support the parallel execution policies: `seq`, `par`, and
`par_unseq` which translate to "sequentially", "parallel" and
"parallel unsequenced".

**Documentation:**
[`std::execution::sequenced_policy`, `std::execution::parallel_policy`, `std::execution::parallel_unsequenced_policy`, `std::execution::unsequenced_policy`](https://en.cppreference.com/w/cpp/algorithm/execution_policy_tag_t)

**Notes:**
*** promo
Banned because
[libc++ support is incomplete](https://libcxx.llvm.org/Status/PSTL.html) and the
interaction of its threading implementation with Chrome's is unclear. Prefer to
explicitly parallelize long-running algorithms using Chrome's threading APIs, so
the same scheduler controls, shutdown policies, tracing, etc. apply as in any
other multithreaded code.
***

### std::aligned_alloc <sup>[banned]</sup>

```c++
int* p2 = static_cast<int*>(std::aligned_alloc(1024, 1024));
```

**Description:** Allocates uninitialized storage with the specified alignment.

**Documentation:**
[`std::aligned_alloc`](https://en.cppreference.com/w/cpp/memory/c/aligned_alloc)

**Notes:**
*** promo
[Will be allowed soon](https://crbug.com/1412818); for now, use
`base::AlignedAlloc`.
***

### std::any <sup>[banned]</sup>

```c++
std::any x = 5;
```

**Description:** A type-safe container for single values of any type.

**Documentation:**
[`std::any`](https://en.cppreference.com/w/cpp/utility/any)

**Notes:**
*** promo
Banned since workaround for lack of RTTI
[isn't compatible with the component build](https://crbug.com/1096380). See also
`absl::any`.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/00cpZ07nye4)
***

### std::byte <sup>[banned]</sup>

```c++
std::byte b = 0xFF;
int i = std::to_integer<int>(b);  // 0xFF
```

**Description:** The contents of a single memory unit. `std::byte` has the same
size and aliasing rules as `unsigned char`, but does not semantically represent
a character or arithmetic value, and does not expose operators other than
bitwise ops.

**Documentation:**
[`std::byte`](https://en.cppreference.com/w/cpp/types/byte)

**Notes:**
*** promo
Banned due to low marginal utility in practice, high conversion costs, and
programmer confusion about "byte" vs. "octet". Use `uint8_t` for the common case
of "8-bit unsigned value", and `char` for the atypical case of code that works
with memory without regard to its contents' values or semantics (e.g allocator
implementations).

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/bBY0gZa1Otk)
***

### std::filesystem <sup>[banned]</sup>

```c++
#include <filesystem>
```

**Description:** A standard way to manipulate files, directories, and paths in a
filesystem.

**Documentation:**
[Filesystem library](https://en.cppreference.com/w/cpp/filesystem)

**Notes:**
*** promo
Banned by the [Google Style Guide](https://google.github.io/styleguide/cppguide.html#Other_Features).
***

### std::{from,to}_chars <sup>[banned]</sup>

```c++
std::from_chars(str.data(), str.data() + str.size(), result);
std::to_chars(str.data(), str.data() + str.size(), 42);
```

**Description:** Locale-independent, non-allocating, non-throwing functions to
convert values from/to character strings, designed for use in high-throughput
contexts.

**Documentation:**
[`std::from_chars`](https://en.cppreference.com/w/cpp/utility/from_chars)
[`std::to_chars`](https://en.cppreference.com/w/cpp/utility/to_chars),

**Notes:**
*** promo
Overlaps with utilities in `base/strings/string_number_conversions.h`, which are
easier to use correctly.
***

### std::{pmr::memory_resource,polymorphic_allocator} <sup>[banned]</sup>

```c++
#include <memory_resource>
```

**Description:** Manages memory allocations using runtime polymorphism.

**Documentation:**
[`std::pmr::memory_resource`](https://en.cppreference.com/w/cpp/memory/memory_resource),
[`std::pmr::polymorphic_allocator`](https://en.cppreference.com/w/cpp/memory/polymorphic_allocator)

**Notes:**
*** promo
Banned because Chromium does not customize allocators
([PartitionAlloc](https://chromium.googlesource.com/chromium/src/+/main/base/allocator/partition_allocator/PartitionAlloc.md)
is used globally).
***

### std::timespec_get <sup>[banned]</sup>

```c++
std::timespec ts;
std::timespec_get(&ts, TIME_UTC);
```

**Description:** Gets the current calendar time in the given time base.

**Documentation:**
[`std::timespec_get`](https://en.cppreference.com/w/cpp/chrono/c/timespec_get)

**Notes:**
*** promo
Banned due to unclear, implementation-defined behavior. On POSIX, use
`base::TimeDelta::ToTimeSpec()`; this could be supported on other platforms if
desirable.
***

### std::uncaught_exceptions <sup>[banned]</sup>

```c++
int count = std::uncaught_exceptions();
```

**Description:** Determines whether there are live exception objects.

**Documentation:**
[`std::uncaught_exceptions`](https://en.cppreference.com/w/cpp/error/uncaught_exception)

**Notes:**
*** promo
Banned because exceptions are banned.
***

### Transparent std::owner_less <sup>[banned]</sup>

```c++
std::map<std::weak_ptr<T>, U, std::owner_less<>>
```

**Description:** Function object providing mixed-type owner-based ordering of
shared and weak pointers, regardless of the type of the pointee.

**Documentation:**
[`std::owner_less`](https://en.cppreference.com/w/cpp/memory/owner_less)

**Notes:**
*** promo
Banned since `std::shared_ptr` and `std::weak_ptr` are banned.
***

### weak_from_this <sup>[banned]</sup>

```c++
auto weak_ptr = weak_from_this();
```

**Description:** Returns a `std::weak_ptr<T>` that tracks ownership of `*this`
by all existing `std::shared_ptr`s that refer to `*this`.

**Documentation:**
[`std::enable_shared_from_this<T>::weak_from_this`](https://en.cppreference.com/w/cpp/memory/enable_shared_from_this/weak_from_this)

**Notes:**
*** promo
Banned since `std::shared_ptr` and `std::weak_ptr` are banned.
***

## C++20 Banned Language Features {#core-blocklist-20}

The following C++20 language features are not allowed in the Chromium codebase.

### char8_t <sup>[banned]</sup>

```c++
char8_t c = u8'x';
```

**Description:** A single UTF-8 code unit. Similar to `unsigned char`, but
considered a distinct type.

**Documentation:**
[Fundamental types](https://en.cppreference.com/w/cpp/language/types#char8_t)

**Notes:**
*** promo
Use `char` and unprefixed character literals. Non-UTF-8 encodings are rare
enough in Chromium that the value of distinguishing them at the type level is
low, and `char8_t*` is not interconvertible with `char*` (what ~all Chromium,
STL, and platform-specific APIs use), so using `u8` prefixes would obligate us
to insert casts everywhere. If you want to declare at a type level that a block
of data is string-like and not an arbitrary binary blob, prefer
`std::string[_view]` over `char*`.
***

### Coroutines <sup>[banned]</sup>

```c++
co_return 1;
```

**Description:** Allows writing functions that logically block while physically
returning control to a caller. This enables writing some kinds of async code in
simple, straight-line ways without storing state in members or binding
callbacks.

**Documentation:**
[Coroutines](https://en.cppreference.com/w/cpp/language/coroutines)

**Notes:**
*** promo
Requires significant support code and planning around API and migration, see
[discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ehMerLApxr8/m/5vzoHqCgCwAJ).
Re-evaluate once supported on win32 and there is a plan for how to integrate
with Chrome task scheduling.
***

### Modules <sup>[banned]</sup>

```c++
export module helloworld; // module declaration

import <iostream>;        // import declaration

export void hello() {     // export declaration
  std::cout << "Hello world!\n";
}
```

**Description:** Modules provide an alternative to many uses of headers which
allows for faster compilation, better tooling support, and reduction of problems
like "include what you use".

**Documentation:**
[Modules](https://en.cppreference.com/w/cpp/language/modules)

**Notes:**
*** promo
Not yet sufficiently supported in Clang and GN. Re-evaluate when support
improves.
***

### [[no_unique_address]] <sup>[banned]</sup>

```c++
struct Empty {};
struct X {
  int i;
  [[no_unique_address]] Empty e;
};
```

**Description:** Allows a data member to be overlapped with other members.

**Documentation:**
[C++ attribute: `no_unique_address`](https://en.cppreference.com/w/cpp/language/attributes/no_unique_address)

**Notes:**
*** promo
Has no effect on Windows, for compatibility with Microsoft's ABI. Use
`NO_UNIQUE_ADDRESS` from `base/compiler_specific.h` instead. Do not use (either
form) on members of unions due to
[potential memory safety problems](https://github.com/llvm/llvm-project/issues/60711).
***

## C++20 Banned Library Features {#library-blocklist-20}

The following C++20 library features are not allowed in the Chromium codebase.

### std::bind_front <sup>[banned]</sup>

```c++
int minus(int a, int b);
auto fifty_minus_x = std::bind_front(minus, 50);
int forty = fifty_minus_x(10);
```

**Description:** An updated version of `std::bind` with fewer gotchas, similar
to `absl::bind_front`.

**Documentation:**
[`std::bind_front`, `std::bind_back`](https://en.cppreference.com/w/cpp/utility/functional/bind_front)

**Notes:**
*** promo
Overlaps with `base::Bind`.
***

### std::bit_cast <sup>[banned]</sup>

```c++
float quake_rsqrt(float number) {
  long i = std::bit_cast<long>(number);
  i = 0x5f3759df - (i >> 1);  // wtf?
  float y = std::bit_cast<float>(i);
  return y * (1.5f - (0.5f * number * y * y));
}
```

**Description:** Returns an value constructed with the same bits as an value of
a different type.

**Documentation:**
[`std::bit_cast`](https://en.cppreference.com/w/cpp/numeric/bit_cast)

**Notes:**
*** promo
The `std::` version of `bit_cast` allows casting of pointer and reference types,
which is both useless in that it doesn't avoid UB, and dangerous in that it
allows arbitrary casting away of modifiers like `const`. Instead of using
`bit_cast` on pointers, use standard C++ casts. For use on values, use
`base::bit_cast` which does not allow this unwanted usage.
***

### std::{c8rtomb,mbrtoc8} <sup>[banned]</sup>

```c++
std::u8string_view strv = u8"zß水🍌";
std::mbstate_t state;
char out[MB_LEN_MAX] = {0};
for (char8_t c : strv) {
  size_t rc = std::c8rtomb(out, c, &state);
  ...
```

**Description:** Converts a code point between UTF-8 and a multibyte character
encoded using the current C locale.

**Documentation:**
[`std::c8rtomb`](https://en.cppreference.com/w/cpp/string/multibyte/c8rtomb),
[`std::mbrtoc8`](https://en.cppreference.com/w/cpp/string/multibyte/mbrtoc8)

**Notes:**
*** promo
Chromium functionality should not vary with the C locale.
***

### &lt;coroutine&gt; <sup>[banned]</sup>

```c++
#include <coroutine>
```

**Description:** Header which defines various core coroutine types.

**Documentation:**
[Coroutine support](https://en.cppreference.com/w/cpp/coroutine)

**Notes:**
*** promo
See notes on "Coroutines" above.
***

### &lt;format&gt; <sup>[banned]</sup>

```c++
std::cout << std::format("Hello {}!\n", "world");
```

**Description:** Utilities for producing formatted strings.

**Documentation:**
[Formatting library](https://en.cppreference.com/w/cpp/utility/format)

**Notes:**
*** promo
Has both pros and cons compared to `absl::StrFormat` (which we use internally in
base::StringPrintf). Migration would be nontrivial due to string format
specifiers being different. The external {fmt} library might be a better
candidate to switch to.
***

### Range factories and range adaptors <sup>[banned]</sup>

```c++
// Prints 1, 2, 3, 4, 5, 6.
for (auto i : std::ranges::iota_view(1, 7)) {
  std::cout << i << '\n';
}

constexpr int kArr[] = {6, 2, 8, 4, 4, 2};
constexpr auto plus_one = std::views::transform([](int n){ return n + 1; });
static_assert(std::ranges::equal(kArr | plus_one, {7, 3, 9, 5, 5, 3}));
```

**Description:** Lightweight objects that represent iterable sequences.
Provides facilities for lazy operations on ranges, along with composition into
pipelines.

**Documentation:**
[Ranges library](https://en.cppreference.com/w/cpp/ranges)

**Notes:**
*** promo
Banned in Chrome due to questions about the design, impact on build time, and
runtime performance. Note that `std::views::reverse`, `std::views::zip`, and
`std::views::as_rvalue` (along with their `std::ranges::*_view` types) are
explicitly allowed exceptions. Because `std::ranges::views` is an alias for
`std::views`, the exact same exceptions apply when using the
`std::ranges::views::` namespace.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZnIbkfJ0Glw),
[allowed thread](https://groups.google.com/a/chromium.org/g/cxx/c/rfMV-DxJ4Cg)
***

### std::ranges::operator| <sup>[banned]</sup>

```c++
constexpr int kArr[] = {6, 2, 8, 4, 4, 2};
constexpr auto plus_one = std::views::transform([](int n){ return n + 1; });
static_assert(std::ranges::equal(kArr | plus_one, {7, 3, 9, 5, 5, 3}));
```

**Description:** The pipe operator for chaining range adaptor closure objects.

**Documentation:**
[`std::ranges::operator|`](https://en.cppreference.com/w/cpp/named_req/RangeAdaptorClosureObject.html)

**Notes:**
*** promo
Banned in Chromium since range factories and adapters are banned. Explicitly
enforced by the Chromium style clang-plugin. Note that `std::views::reverse`,
`std::views::zip`, and `std::views::as_rvalue` (along with their
`std::ranges::*_view` types) are explicitly allowed exceptions. However, pipe
chaining remains banned even for allowed adaptors; because `std::ranges::views`
is an alias for `std::views`, the exact same exceptions and pipe-chaining
prohibitions apply when using the `std::ranges::views::` namespace
(function call syntax must be used).

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZnIbkfJ0Glw),
[allowed thread](https://groups.google.com/a/chromium.org/g/cxx/c/rfMV-DxJ4Cg),
[operator| thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZzSLYf6-KwQ)
***

### std::ranges::view_interface <sup>[banned]</sup>

```c++
class MyView : public std::ranges::view_interface<MyView> { ... };
```

**Description:** CRTP base class for implementing custom view objects.

**Documentation:**
[`std::ranges::view_interface`](https://en.cppreference.com/w/cpp/ranges/view_interface)

**Notes:**
*** promo
Banned in Chrome since range factories and adapters are banned, and this would
primarily allow authors to create similar functionality.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZnIbkfJ0Glw)
***

### &lt;source_location&gt; <sup>[banned]</sup>

```c++
#include <source_location>
```

**Description:** Provides a class that can hold source code details such as
filenames, function names, and line numbers.

**Documentation:**
[Standard library header `<source_location>`](https://en.cppreference.com/w/cpp/header/source_location)

**Notes:**
*** promo
Regresses binary size vs. `base::Location`, see
[discussion here](https://groups.google.com/a/chromium.org/g/cxx/c/2BsvGgWfNWo/m/2SO4F2b4AAAJ).
***

### &lt;span&gt; <sup>[banned]</sup>

```c++
#include <span>
```

**Description:** Utilities for non-owning views over a sequence of objects.

**Documentation:**
[](https://en.cppreference.com/w/cpp/header/span)

**Notes:**
*** promo
Superseded by `base::span`, which has a richer functionality set.
***

### std::to_address <sup>[banned]</sup>

```c++
std::vector<int> numbers;
int* i = std::to_address(numbers.begin());
```

**Description:** Converts a pointer-like object to a pointer, even if the
pointer does not refer to a constructed object (in which case an expression like
`&*p` is UB).

**Documentation:**
[`std::to_address`](https://en.cppreference.com/w/cpp/memory/to_address)

**Notes:**
*** promo
Banned because it is not guaranteed to be SFINAE-compatible. Use
base::to_address, which does guarantee this.
***

### std::u8string <sup>[banned]</sup>

```c++
std::u8string str = u8"Foo";
```

**Description:** A string whose character type is `char8_t`, intended to hold
UTF-8-encoded text.

**Documentation:**
[`std::basic_string`](https://en.cppreference.com/w/cpp/string/basic_string)

**Notes:**
*** promo
See notes on `char8_t` above.
***

### &lt;syncstream&gt; <sup>[banned]</sup>

```c++
#include <syncstream>
```

**Description:** Facilities for multithreaded access to streams.

**Documentation:**
[Standard library header `<syncstream>`](https://en.cppreference.com/w/cpp/header/syncstream)

**Notes:**
*** promo
Banned due to being unimplemented per
[the libc++ C++20 status page](https://libcxx.llvm.org/Status/Cxx20.html).
Reevaluate usefulness once implemented.
***

## C++23 Allowed Language Features {#core-allowlist-23}

The following C++23 language features are allowed in the Chromium codebase.

### #elifdef, #elifndef <sup>[allowed]</sup>

```c++
#ifdef FOO
...
#elifdef BAR  // New.
...
#elifndef BAZ  // New.
...
#endif
```

**Description:** New conditional inclusion preprocessor directives.

**Documentation:**
[Conditional inclusion](https://en.cppreference.com/w/cpp/preprocessor/conditional.html)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/V6YD6hOjnE8)
***

### if consteval <sup>[allowed]</sup>

```c++
if consteval {
  ...
}
```

**Description:** consteval if statement. This removes some gotchas with
[std::is_constant_evaluated()](https://en.cppreference.com/w/cpp/types/is_constant_evaluated.html#Notes),
which needs to be used with a runtime if (rather than constexpr if) to be
meaningful.

**Documentation:**
[if statement](https://en.cppreference.com/w/cpp/language/if)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/1e90vNHYVFc)
***

### Static operators () and [] <sup>[allowed]</sup>

```
struct FooHash {
  // Does not take an implicit pointer to `this`, which would have been useless.
  static size_t operator()(const Foo& foo) {
    return ...;
  }
};
```

**Description:** Static operators () and []

**Documentation:**
[Operator overloading](https://en.cppreference.com/w/cpp/language/operators.html)

**Notes:**
*** promo
Avoids unnecessary `this` argument for functors, improving performance.
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/_-d7yyX8EeU).
***

## C++23 Allowed Library Features {#library-allowlist-23}

The following C++23 library features are allowed in the Chromium codebase.

### std::byteswap <sup>[allowed]</sup>

```c++
auto x = std::byteswap(y);
```

**Description:** Reverses the bytes of an integer.

**Documentation:**
[std::byteswap](https://en.cppreference.com/w/cpp/numeric/byteswap)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/U2zUF-xOj6A/m/ZiRRZdr7AwAJ)
***

### Various new ranges algorithms <sup>[allowed]</sup>

```c++
std::ranges::contains, std::ranges::contains_subrange
std::ranges::starts_with
std::ranges::ends_with
std::ranges::find_last, std::ranges::find_last_if, std::ranges::find_last_if_not
std::ranges::iota
std::ranges::fold_left
std::ranges::fold_left_with_iter
// The ones below are pending libc++ implementation as of 01/2026.
std::ranges::shift_left, std::ranges::shift_right
std::ranges::fold_left_first
std::ranges::fold_right
std::ranges::fold_right_last
std::ranges::fold_left_first_with_iter
```

**Description:** Various new ranges algorithms

**Documentation:**
[`<algorithm>`](https://en.cppreference.com/w/cpp/header/algorithm.html)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/wZg3s5m6rOE).
Migration of base::Contains() tracked [here](https://crbug.com/470391351).
***

### std::out_ptr, std::inout_ptr <sup>[allowed]</sup>

```c++
std::unique_ptr<T> p;
void GetT(T** out);
GetT(std::out_ptr(p));
```

**Description:** Smart pointer adapters for functions that take raw pointers as
out-parameters.

**Documentation:**
[std::out_ptr](https://en.cppreference.com/w/cpp/memory/out_ptr_t/out_ptr),
[std::inout_ptr](https://en.cppreference.com/w/cpp/memory/inout_ptr_t/inout_ptr)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/wQaVa2yNnNo).
***

### std::ranges::to <sup>[allowed]</sup>

```c++
std::set<int> s = {1, 2, 3};
auto u = std::ranges::to<std::vector>(s);
```

**Description:** Converts a range to a container.

**Documentation:**
[std::ranges::to](https://en.cppreference.com/w/cpp/ranges/to)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZzSLYf6-KwQ).
NOTE: `std::ranges::to` could also be used as a range adaptor, but those are
banned in Chromium, see [here](#range-factories-and-range-adaptors-banned).
Furthermore, `operator|` chaining is banned even when combining with allowed
adaptors like `std::views::as_rvalue` (always use function call syntax:
`std::ranges::to<std::vector>(std::views::as_rvalue(range))`).
***

### std::views::as_rvalue <sup>[allowed]</sup>

```c++
// Before:
for (auto&& item : base::RangeAsRvalues(source)) { ... }

// After:
for (auto&& item : std::views::as_rvalue(source)) { ... }
```

**Description:** A view adapter that presents each element of a range as an
rvalue reference, moving elements out of the underlying sequence.

**Documentation:**
[`std::views::as_rvalue`](https://en.cppreference.com/w/cpp/ranges/as_rvalue_view)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/rfMV-DxJ4Cg).
***

### std::views::zip <sup>[allowed]</sup>

```c++
// Before:
for (auto [a, b] : base::zip(vec_a, vec_b)) { ... }
// or:
for (auto [a, b] : base::Zip(vec_a, vec_b)) { ... }

// After:
for (auto [a, b] : std::views::zip(vec_a, vec_b)) { ... }
```

**Description:** A view adapter that iterates over multiple ranges in parallel,
producing a tuple of references to the elements of the underlying sequences.

**Documentation:**
[`std::views::zip`](https://en.cppreference.com/w/cpp/ranges/zip_view)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/rfMV-DxJ4Cg).
***

### std::to_underlying <sup>[allowed]</sup>

```c++
auto x = std::to_underlying(enum_val);
```

**Description:** Converts an enumeration to its underlying type.

**Documentation:**
[std::to_underlying](https://en.cppreference.com/w/cpp/utility/to_underlying)

**Notes:**
*** promo
Migration from `base::to_underlying` is tracked in https://crbug.com/470039537.
***

## C++23 TBD Language Features {#core-review-23}

The following C++23 language features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### Explicit object parameter <sup>[tbd]</sup>

```c++
struct S {
  void f(this S& self);
};
```

**Description:** Allows explicit specification of the object parameter (deducing `this`) in member functions.

**Documentation:**
[Explicit object parameter](https://en.cppreference.com/w/cpp/language/member_functions#Explicit_object_parameter)

**Notes:**
*** promo
None
***

### Multidimensional subscript operator <sup>[tbd]</sup>

```c++
struct S {
  int operator[](int i, int j);
};
```

**Description:** Allows multiple arguments in the subscript operator.

**Documentation:**
[Operator overloading](https://en.cppreference.com/w/cpp/language/operators#Array_subscript_operator)

**Notes:**
*** promo
None
***

### auto(x), auto{x} <sup>[tbd]</sup>

```c++
void f(const int& i) {
  auto copy = auto(i);
}
```

**Description:** Prvalue copy (decay-copy).

**Documentation:**
[Functional cast](https://en.cppreference.com/w/cpp/language/explicit_cast)

**Notes:**
*** promo
None
***

### [[assume]] <sup>[tbd]</sup>

```c++
[[assume(n > 0)]];
```

**Description:** Provides a hint to the optimizer.

**Documentation:**
[dcl.attr.assume](https://en.cppreference.com/w/cpp/language/attributes/assume)

**Notes:**
*** promo
None
***

### #warning <sup>[tbd]</sup>

```c++
#warning "This is a warning"
```

**Description:** Standardized preprocessor warning directive.

**Documentation:**
[#warning](https://en.cppreference.com/w/cpp/preprocessor/warning)

**Notes:**
*** promo
This was standardized in C++23, but was already supported by clang.
From thakis@: #warning doesn't honor -Werror, and we want things to either be
errors, or silent.
***

### Literal suffix for size_t <sup>[tbd]</sup>

```c++
auto s = 10uz;
```

**Description:** Literal suffix `z` or `uz` for `std::size_t`.

**Documentation:**
[Integer literal](https://en.cppreference.com/w/cpp/language/integer_literal)

**Notes:**
*** promo
None
***

### Named character escapes <sup>[tbd]</sup>

```c++
auto c = "\N{LATIN CAPITAL LETTER A}";
```

**Description:** Universal character names using `\N{...}`.

**Documentation:**
[Escape sequences](https://en.cppreference.com/w/cpp/language/escape)

**Notes:**
*** promo
None
***

## C++23 TBD Library Features {#library-review-23}

The following C++23 library features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### std::expected <sup>[tbd]</sup>

```c++
std::expected<int, std::string> e;
```

**Description:** A vocabulary type that contains an expected value or an error.

**Documentation:**
[std::expected](https://en.cppreference.com/w/cpp/utility/expected)

**Notes:**
*** promo
Overlaps with `base::expected`.
***

### std::flat_map, std::flat_multimap, std::flat_set, std::flat_multiset <sup>[tbd]</sup>

```c++
std::flat_map<int, std::string> map;
```

**Description:** Container adaptors that provide the functionality of associative containers using sorted vectors.

**Documentation:**
[std::flat_map](https://en.cppreference.com/w/cpp/container/flat_map)
[std::flat_multimap](https://en.cppreference.com/w/cpp/container/flat_multimap)
[std::flat_set](https://en.cppreference.com/w/cpp/container/flat_set)
[std::flat_multiset](https://en.cppreference.com/w/cpp/container/flat_multiset)

**Notes:**
*** promo
Overlaps with `base::flat_map` and `base::flat_set`.
***

### std::mdspan <sup>[tbd]</sup>

```c++
std::mdspan m(ptr, 10, 10);
```

**Description:** Multidimensional array view.

**Documentation:**
[std::mdspan](https://en.cppreference.com/w/cpp/container/mdspan)

**Notes:**
*** promo
We ban std::span in favor of base::span, which has better safety guarantees.
If we want to support this, maybe we should implement base::mdspan.
***

### Range Formatting <sup>[tbd]</sup>

```c++
LOG(INFO) << std::format("Values: {}", my_vector);
```

**Description:** Extends `<format>` to support printing containers and ranges.

**Documentation:**
[`<format>`](https://en.cppreference.com/w/cpp/utility/format/format.html)

**Notes:**
*** promo
As of 01/2026, `<format>` is still a TBD feature from C++20.
***

### std::print <sup>[tbd]</sup>

```c++
std::print("Hello {}", "world");
```

**Description:** Formatted output.

**Documentation:**
[Print functions](https://en.cppreference.com/w/cpp/io/print)

**Notes:**
*** promo
Overlaps with LOG() and friends.
***

### std::generator <sup>[tbd]</sup>

```c++
std::generator<int> gen() { co_yield 1; }
```

**Description:** Coroutine generator.

**Documentation:**
[std::generator](https://en.cppreference.com/w/cpp/coroutine/generator)

**Notes:**
*** promo
As of 01/2026, coroutine support in Chromium is still TBD.
***

### std::stacktrace <sup>[tbd]</sup>

```c++
auto trace = std::stacktrace::current();
```

**Description:** Captures a stack trace.

**Documentation:**
[std::stacktrace](https://en.cppreference.com/w/cpp/utility/stacktrace)

**Notes:**
*** promo
Overlaps with `base::debug::StackTrace`.
***

### std::move_only_function <sup>[tbd]</sup>

```c++
std::move_only_function<void()> f = [x = std::make_unique<int>(1)] {};
```

**Description:** Function wrapper for move-only objects.

**Documentation:**
[std::move_only_function](https://en.cppreference.com/w/cpp/utility/functional/move_only_function)

**Notes:**
*** promo
Overlaps with `base::OnceCallback`.
***

### std::unreachable <sup>[tbd]</sup>

```c++
std::unreachable();
```

**Description:** Indicates a codepath that is unreachable and invokes undefined
behavior if executed.

**Documentation:**
[std::unreachable](https://en.cppreference.com/w/cpp/utility/unreachable)

**Notes:**
*** promo
Standard version of `__builtin_unreachable()`. Unlike `NOTREACHED()`, which
cleanly aborts, this hints the compiler to optimize assuming this point is
unreachable, with no bounds on how the program behaves if the annotation was
wrong. This and `__builtin_unreachable()` should only be used in rare
circumstances.
***

### std::spanstream <sup>[tbd]</sup>

```c++
std::spanstream s(buffer);
```

**Description:** Input/output stream using a span as buffer.

**Documentation:**
[std::spanstream](https://en.cppreference.com/w/cpp/header/spanstream.html)

**Notes:**
*** promo
None
***

### Fixed width floating-point types <sup>[tbd]</sup>

```c++
#include <stdfloat>

int main()
{
    std::float64_t f = 0.1f64;
}
```

**Description:** Similar to int32_t and friends but for floats.

**Documentation:**
[<stdfloat>](https://en.cppreference.com/w/cpp/types/floating-point.html#Fixed_width_floating-point_types)

**Notes:**
*** promo
None
***

### std::start_lifetime_as <sup>[tbd]</sup>

```c++
void* storage = std::malloc(sizeof(T));
T* p = std::start_lifetime_as<T>(storage);
```

**Description:** Explicitly starts the lifetime of an object of type T in the
given storage.

**Documentation:**
[std::start_lifetime_as](https://en.cppreference.com/w/cpp/memory/start_lifetime_as)

**Notes:**
*** promo
None
***

## Abseil Banned Library Features {#absl-blocklist}

The following Abseil library features are not allowed in the Chromium codebase.

### Any <sup>[banned]</sup>

```c++
absl::any a = int{5};
EXPECT_THAT(absl::any_cast<int>(&a), Pointee(5));
EXPECT_EQ(absl::any_cast<size_t>(&a), nullptr);
```

**Description:** Early adaptation of C++17 `std::any`.

**Documentation:** [std::any](https://en.cppreference.com/w/cpp/utility/any)

**Notes:**
*** promo
Banned since workaround for lack of RTTI
[isn't compatible with the component build](https://crbug.com/1096380). See also
`std::any`.
***

### AnyInvocable <sup>[banned]</sup>

```c++
absl::AnyInvocable
```

**Description:** An equivalent of the C++23 std::move_only_function.

**Documentation:**
*   [any_invocable.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/any_invocable.h)
*   [std::move_only_function](https://en.cppreference.com/w/cpp/utility/functional/move_only_function/move_only_function)

**Notes:**
*** promo
Banned due to overlap with `base::RepeatingCallback`, `base::OnceCallback`.
***

### Attributes <sup>[banned]</sup>

```c++
T* data() ABSL_ATTRIBUTE_LIFETIME_BOUND { return data_; }
ABSL_ATTRIBUTE_NO_TAIL_CALL ReturnType Loop();
struct S { bool b; int32_t i; } ABSL_ATTRIBUTE_PACKED;
```

**Description:** Cross-platform macros to expose compiler-specific
functionality.

**Documentation:** [attributes.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/base/attributes.h)

**Notes:**
*** promo
Long names discourage use. Use standardized attributes over macros where
possible, and otherwise prefer shorter alternatives in
`base/compiler_specific.h`.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/lVQOJTng1RU)
***

### btree_\* containers <sup>[banned]</sup>

```c++
absl::btree_map
absl::btree_set
absl::btree_multimap
absl::btree_multiset
```

**Description:** Alternatives to the tree-based standard library containers
designed to be more efficient in the general case.

**Documentation:** [Containers](https://abseil.io/docs/cpp/guides/container)

**Notes:**
*** promo
In theory these should be superior alternatives that could replace most uses of
`std::map` and company. In practice they have been found to introduce a
substantial code size increase. Until this problem can be resolved the use of
these containers is banned. Use the standard library containers instead.
***

### bind_back and bind_front <sup>[banned]</sup>

```c++
absl::bind_back
absl::bind_front
```

**Description:** Binds the first (bind_front) or the last (bind_back) N arguments
of an invocable object and stores them by value.

**Documentation:**
*   [bind_back.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/bind_back.h)
*   [bind_front.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/bind_front.h)
*   [Avoid std::bind](https://abseil.io/tips/108)

**Notes:**
*** promo
Banned due to overlap with `base::Bind`.
***

### Command line flags <sup>[banned]</sup>

```c++
ABSL_FLAG(bool, logs, false, "print logs to stderr");
app --logs=true;
```

**Description:** Allows programmatic access to flag values passed on the
command-line to binaries.

**Documentation:** [Flags Library](https://abseil.io/docs/cpp/guides/flags)

**Notes:**
*** promo
Banned since workaround for lack of RTTI
[isn't compatible with the component build](https://crbug.com/1096380). Use
`base::CommandLine` instead.
***

### Container utilities <sup>[banned]</sup>

```c++
auto it = absl::c_find(container, value);
```

**Description:** Container-based versions of algorithmic functions within C++
standard library.

**Documentation:**
[container.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/algorithm/container.h)

**Notes:**
*** promo
Superseded by algorithms in `std::ranges::`.
***

### FixedArray <sup>[banned]</sup>

```c++
absl::FixedArray<MyObj> objs_;
```

**Description:** A fixed size array like `std::array`, but with size determined
at runtime instead of compile time.

**Documentation:**
[fixed_array.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/container/fixed_array.h)

**Notes:**
*** promo
Direct construction is banned due to the risk of UB with uninitialized
trivially-default-constructible types. Instead use `base/types/fixed_array.h`,
which is a light-weight wrapper that deletes the problematic constructor.
***

### FunctionRef <sup>[banned]</sup>

```c++
absl::FunctionRef
```

**Description:** Type for holding a non-owning reference to an object of any
invocable type.

**Documentation:**
[function_ref.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/function_ref.h)

**Notes:**
*** promo
- `absl::FunctionRef` is banned due to allowing implicit conversions between
  function signatures in potentially surprising ways. For example, a callable
  with the signature `int()` will bind to `absl::FunctionRef<void()>`: the
  return value from the callable will be silently discarded.
- In Chromium, use `base::FunctionRef` instead.
- Unlike `base::OnceCallback` and `base::RepeatingCallback`, `base::FunctionRef`
  supports capturing lambdas.
- Useful when passing an invocable object to a function that synchronously calls
  the invocable object, e.g. `ForEachFrame(base::FunctionRef<void(Frame&)>)`.
  This can often result in clearer code than code that is templated to accept
  lambdas, e.g. with `template <typename Invocable> void
  ForEachFrame(Invocable invocable)`, it is much less obvious what arguments
  will be passed to `invocable`.
- For now, `base::OnceCallback` and `base::RepeatingCallback` intentionally
  disallow conversions to `base::FunctionRef`, under the theory that the
  callback should be a capturing lambda instead. Attempting to use this
  conversion will trigger a `static_assert` requesting additional feedback for
  use cases where this conversion would be valuable.
- *Important:* `base::FunctionRef` must not outlive the function call. Like
  `std::string_view`, `base::FunctionRef` is a *non-owning* reference. Using a
  `base::FunctionRef` as a return value or class field is dangerous and likely
  to result in lifetime bugs.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/JVN4E4IIYA0)
***

### Log macros and related classes <sup>[banned]</sup>

```c++
LOG(INFO) << message;
CHECK(condition);
absl::AddLogSink(&custom_sink_to_capture_absl_logs);
```

**Description:** Macros and related classes to perform debug loggings

**Documentation:**
[log.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/log/log.h)
[check.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/log/check.h)

**Notes:**
*** promo
Banned due to overlap with `base/logging.h`. We'd like to drop Chromium's
version and replace with the Abseil one, but no one has looked into how to
migrate and what impacts (e.g. build time) we'd incur. If you'd like to do this
work, please contact cxx@.
***

### NoDestructor <sup>[banned]</sup>

```c++
// Global or namespace scope.
ABSL_CONST_INIT absl::NoDestructor<MyRegistry> reg{"foo", "bar", 8008};

// Function scope.
const std::string& MyString() {
  static const absl::NoDestructor<std::string> x("foo");
  return *x;
}
```

**Description:** `absl::NoDestructor<T>` is a wrapper around an object of
type T that behaves as an object of type T but never calls T's destructor.

**Documentation:**
[no_destructor.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/base/no_destructor.h)

**Notes:**
*** promo
Overlaps with `base::NoDestructor`. Banned pending rewriting friending of that
class into a form usable with this (see
[crbug.com/392931072](https://crbug.com/392931072)); at that point we can allow
this and migrate to it.
***

### Nullability annotations <sup>[banned]</sup>

```c++
void PaySalary(Employee* absl_nonnull employee) {
  pay(*employee);  // OK to dereference
}
```

**Description:** Annotations to more clearly specify contracts

**Documentation:**
[nullability.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/base/nullability.h)

**Notes:**
*** promo
Banned due to no feasible path to codebase-wide use and little mechanism for
enforcement.
***

### Optional <sup>[banned]</sup>

```c++
absl::optional<int> Func(bool b) {
  return b ? absl::make_optional(1) : absl::nullopt;
}
```

**Description:** Early adaptation of C++17 `std::optional`.

**Documentation:** [std::optional](https://en.cppreference.com/w/cpp/utility/optional)

**Notes:**
*** promo
Superseded by `std::optional`. Use `std::optional` instead.
***

### Random <sup>[banned]</sup>

```c++
absl::BitGen bitgen;
size_t index = absl::Uniform(bitgen, 0u, elems.size());
```

**Description:** Functions and utilities for generating pseudorandom data.

**Documentation:** [Random library](https://abseil.io/docs/cpp/guides/random)

**Notes:**
*** promo
Banned because most uses of random values in Chromium should be using a
cryptographically secure generator. Use `base/rand_util.h` instead.
***

### Span <sup>[banned]</sup>

```c++
absl::Span
```

**Description:** Early adaptation of C++20 `std::span`.

**Documentation:** [Using absl::Span](https://abseil.io/tips/93)

**Notes:**
*** promo
Banned due to being less std::-compliant than `base::span`. Keep using
`base::span`.
***

### StatusOr <sup>[banned]</sup>

```c++
absl::StatusOr<T>
```

**Description:** An object that is either a usable value, or an error Status
explaining why such a value is not present.

**Documentation:**
[statusor.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/status/statusor.h)

**Notes:**
*** promo
Overlaps with `base::expected`.
***

### string_view <sup>[banned]</sup>

```c++
absl::string_view
```

**Description:** Early adaptation of C++17 `std::string_view`.

**Documentation:** [absl::string_view](https://abseil.io/tips/1)

**Notes:**
*** promo
Originally banned due to only working with 8-bit characters. Now it is
unnecessary because it is the same type as `std::string_view`.
Please use `std::string_view` instead.
***

### Strings Library <sup>[banned]</sup>

```c++
absl::StrSplit
absl::StrJoin
absl::StrCat
absl::StrAppend
absl::Substitute
absl::StrContains
```

**Description:** Classes and utility functions for manipulating and comparing
strings.

**Documentation:**
[String Utilities](https://abseil.io/docs/cpp/guides/strings)

**Notes:**
*** promo
Overlaps with `base/strings`. We
[should re-evalute](https://bugs.chromium.org/p/chromium/issues/detail?id=1371966)
when we've
[migrated](https://bugs.chromium.org/p/chromium/issues/detail?id=691162) from
`base::StringPiece` to `std::string_view`. Also note that `absl::StrFormat()` is
not considered part of this group, and is explicitly allowed.
***

### Synchronization <sup>[banned]</sup>

```c++
absl::Mutex
```

**Description:** Primitives for managing tasks across different threads.

**Documentation:**
[Synchronization](https://abseil.io/docs/cpp/guides/synchronization)

**Notes:**
*** promo
Overlaps with `base/synchronization/`. We would love
[more testing](https://bugs.chromium.org/p/chromium/issues/detail?id=1371969) on
whether there are compelling reasons to prefer base, absl, or std
synchronization primitives; for now, use `base/synchronization/`.
***

### Time library <sup>[banned]</sup>

```c++
absl::Duration
absl::Time
absl::TimeZone
absl::CivilDay
```

**Description:** Abstractions for holding time values, both in terms of
absolute time and civil time.

**Documentation:** [Time](https://abseil.io/docs/cpp/guides/time)

**Notes:**
*** promo
Overlaps with `base/time/`.
***

### Variant <sup>[banned]</sup>

```c++
absl::bad_variant_access;
absl::get;
absl::get_if;
absl::holds_alternative;
absl::monostate;
absl::variant;
absl::variant_alternative;
absl::variant_alternative_t;
absl::variant_npos;
absl::variant_size;
absl::variant_size_v;
absl::visit;
```

**Description:** A backport of C++17's std::variant type-safe union and related utilities.

**Notes:**
*** promo
These are just aliases to the std counterparts these days. Use std instead.
***

### Utility library <sup>[banned]</sup>

```c++
absl::apply;
absl::exchange;
absl::forward;
absl::in_place;
absl::in_place_index;
absl::in_place_index_t;
absl::in_place_t;
absl::in_place_type;
absl::in_place_type_t;
absl::index_sequence;
absl::index_sequence_for;
absl::integer_sequence;
absl::make_from_tuple;
absl::make_index_sequence;
absl::make_integer_sequence;
absl::move;
```

**Description:** Backports of various C++17 template utilities.

**Notes:**
*** promo
These are just aliases to the std counterparts these days. Use std instead.
***

## Abseil TBD Features {#absl-review}

The following Abseil library features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### absl::AnySpan <sup>[tbd]</sup>

```c++
void MyFunction(AnySpan<const MyMessage> messages);

// Invoke with container of smart pointers.
std::deque<std::unique_ptr<MyMessage>> message_ptrs = ...;
MyFunction(AnySpan<const MyMessage>(message_ptrs, any_span_transform::Deref()));

// Invoke with a repeated proto field of type MyMessage.
OtherMessage proto_message = ...;
MyFunction(proto_message.repeated_field());
```

**Description:** Provides a view of a random access container, much like
absl::Span, except it can handle more container types and perform a variety of
useful transformations.

**Documentation:**
*  [any_span.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/types/any_span.h)


### absl::linked_hash_set, absl::linked_hash_map <sup>[tbd]</sup>

```c++
absl::linked_hash_set<int> m;
m.insert(2);
m.insert(1);
m.insert(3);
EXPECT_THAT(m, ElementsAre(2, 1, 3));
```

**Description:** A simple insertion-ordered set or map. It provides O(1)
amortized insertions and lookups, as well as iteration in the insertion order.

**Documentation:**
*   [linked_hash_set.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/container/linked_hash_set.h)
*   [linked_hash_map.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/container/linked_hash_map.h)


### absl::optional_ref <sup>[tbd]</sup>

```c++
   void ProcessT(absl::optional_ref<const T> input) {
     if (!input.has_value()) {
       // Handle empty case.
       return;
     }
     const T& val = *input;
     // Do something with val.
   }

   ProcessT(std::nullopt);
   ProcessT(BuildT());
```

**Description:** A `std::optional`-like interface around `T*`.
It is similar to C++26's `std::optional<T&>`, but with slight enhancements.

**Documentation:**
*   [optional_ref.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/types/optional_ref.h)


### absl::SourceLocation <sup>[tbd]</sup>

```c++
   void TracedAdd(int i, SourceLocation loc = SourceLocation::current()) {
     std::cout << loc.file_name() << ":" << loc.line() << " added " << i;
     ...
   }

   void UserCode() {
     TracedAdd(1);
     TracedAdd(2);
   }
```

**Description:** provides source-code location info.
It is similar to C++20's `std::source_location`. Unlike std::source_location,
it is not permanently valid and must not outlive its source.

**Documentation:**
*   [source_location.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/types/source_location.h)

**Notes:**
*** promo
Overlaps with `base::Location`.
***
