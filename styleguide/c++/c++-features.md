# Modern C++ use in Chromium

_This document is part of the more general
[Chromium C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md).
It summarizes the supported state of new and updated language and library
features in recent C++ standards and the [Abseil](https://abseil.io/about/)
library. This guide applies to both Chromium and its subprojects, though
subprojects can choose to be more restrictive if necessary for toolchain
support._

The C++ language has in recent years received an updated standard every three
years (C++11, C++14, etc.). For various reasons, Chromium does not immediately
allow new features on the publication of such a standard. Instead, once
Chromium supports the toolchain to a certain extent (e.g., build support is
ready), a standard is declared "_initially supported_", with new
language/library features banned pending discussion but not yet allowed.

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
*   **C++20:** _Initially supported November 13, 2023; see allowed/banned/TBD
    features below_
*   **C++23:** _Not yet officially standardized_
*   **Abseil:** _Default allowed; see banned/TBD features below. The following
    dates represent the start of the two-year TBD periods for certain parts of
    Abseil:_
      * absl::AnyInvocable: Initially added to third_party June 20, 2022
      * Log library: Initially added to third_party Aug 31, 2022
      * CRC32C library: Initially added to third_party Dec 5, 2022
      * Nullability annotation: Initially added to third_party Jun 21, 2023
      * Overload: Initially added to third_party Sep 27, 2023
      * NoDestructor: Initially added to third_party Nov 15, 2023

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

### std::in_place{_type,_index}[_t] <sup>[banned]</sup>

```c++
std::variant<int, float> v{std::in_place_type<int>, 1.4};
```

**Description:** `std::in_place_type` and `std::in_place_index` are
disambiguation tags for `std::variant` and `std::any` to indicate that the
object should be constructed in-place.

**Documentation:**
[`std::in_place_type`](https://en.cppreference.com/w/cpp/utility/in_place)

**Notes:**
*** promo
Banned for now because `std::variant` and `std::any` are banned. Because
`absl::variant` is used instead, and it requires `absl::in_place_type`, use
`absl::in_place_type` for non-Abseil Chromium
code.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZspmuJPpv6s)
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

### std::variant <sup>[banned]</sup>

```c++
std::variant<int, double> v = 12;
```

**Description:** The class template `std::variant` represents a type-safe
`union`. An instance of `std::variant` at any given time holds a value of one of
its alternative types (it's also possible for it to be valueless).

**Documentation:**
[`std::variant`](https://en.cppreference.com/w/cpp/utility/variant)

**Notes:**
*** promo
[Will be allowed soon](https://crbug.com/1373620); for now, use `absl::variant`.
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

## C++20 Allowed Language Features {#core-allowlist-20}

The following C++20 language features are allowed in the Chromium codebase.

### Abbreviated function templates <sup>[allowed]</sup>

```c++
// template <typename T>
// void f1(T x);
void f1(auto x);

// template <C T>  // `C` is a concept
// void f2(T x);
void f2(C auto x);

// template <typename T, C U>  // `C` is a concept
// void f3(T x, U y);
template <typename T>
void f3(T x, C auto y);

// template<typename... Ts>
// void f4(Ts... xs);
void f4(auto... xs);
```

**Description:** Function params of type `auto` become syntactic sugar for
declaring a template type for each such parameter.

**Documentation:**
[Abbreviated function template](https://en.cppreference.com/w/cpp/language/function_template#Abbreviated_function_template)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414526)
***

### consteval <sup>[allowed]</sup>

```c++
consteval int sqr(int n) { return n * n; }
constexpr int kHundred = sqr(10);                  // OK
constexpr int quad(int n) { return sqr(sqr(n)); }  // ERROR, might be runtime
```

**Description:** Specified that a function may only be used in a compile-time
context.

**Documentation:**
[`consteval` specifier](https://en.cppreference.com/w/cpp/language/consteval)

**Notes:**
*** promo
None
***

### Constraints and concepts <sup>[allowed]</sup>

```c++
// `Hashable` is a concept satisfied by any type `T` for which the expression
// `std::hash<T>{}(a)` compiles and produces a value convertible to `size_t`.
template<typename T>
concept Hashable = requires(T a)
{
    { std::hash<T>{}(a) } -> std::convertible_to<size_t>;
};
template <Hashable T>  // Only instantiable for `T`s that satisfy `Hashable`.
void f(T) { ... }
```

**Description:** Allows bundling sets of requirements together as named
concepts, then enforcing them on template arguments.

**Documentation:**
[Constraints and concepts](https://en.cppreference.com/w/cpp/language/constraints)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414528)
***

### Default comparisons <sup>[allowed]</sup>

```c++
class S : public T {
  // Non-member equality operator with access to private members.
  // Compares `T` bases, then `x`, then `y`, short-circuiting when
  // it finds inequality.
  friend bool operator==(const S&, const S&) = default;

  // Non-member ordering operator with access to private members.
  // Compares `T` bases, then `x`, then `y`, short-circuiting when
  // it finds an ordering difference.
  friend auto operator<=>(const S&, const S&) = default;

  int x;
  bool y;
};
```

**Description:** Requests that the compiler generate the implementation of
any comparison operator, including `<=>`. Prefer non-member comparison
operators. When defaulting `<=>`, also explicitly default `==`. Together these
are sufficient to allow any comparison as long as callers do not need to take
the address of any non-declared operator.

**Documentation:**
[Default comparisons](https://en.cppreference.com/w/cpp/language/default_comparisons)

**Notes:**
*** promo
Unlike constructors/destructors, our compiler extensions do not require these
to be written out-of-line in the .cc file. Feel free to write `= default`
directly in the header, as this is much simpler to write.

- [Migration bug](https://crbug.com/1414530)
- [Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/h4lVi2jHU-0/m/X0q_Bh2IAAAJ)

***

### Designated initializers <sup>[allowed]</sup>

```c++
struct S { int x = 1; int y = 2; }
S s{ .y = 3 };  // OK, s.x == 1, s.y == 3
```

**Description:** Allows explicit initialization of subsets of aggregate members
at construction.

**Documentation:**
[Designated initializers](https://en.cppreference.com/w/cpp/language/aggregate_initialization#Designated_initializers)

**Notes:**
*** promo
None
***

### __has_cpp_attribute <sup>[allowed]</sup>

```c++
#if __has_cpp_attribute(assume)  // Toolchain supports C++23 `[[assume]]`.
...
#endif
```

**Description:** Checks whether the toolchain supports a particular standard
attribute.

**Documentation:**
[Feature testing](https://en.cppreference.com/w/cpp/feature_test)

**Notes:**
*** promo
None
***

### constinit <sup>[allowed]</sup>

```c++
constinit int x = 3;
void foo() {
  ++x;
}
```

**Description:** Ensures that a variable can be compile-time initialized. This
is like a milder form of `constexpr` that does not force variables to be const
or have constant destruction.

**Documentation:**
[`constinit` specifier](https://en.cppreference.com/w/cpp/language/constinit)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414612)
***

### Initializers for bit-field members <sup>[allowed]</sup>

```c++
struct S {
  uint32_t x : 27 = 2;
};
```

**Description:** Allows specifying the default initial value of a bit-field
member, as can already be done for other member types.

**Documentation:**
[Bit-field](https://en.cppreference.com/w/cpp/language/bit_field)

**Notes:**
*** promo
None
***

### Lambda captures with initializers that are pack expansions <sup>[allowed]</sup>

```c++
template <typename... Args>
void foo(Args... args) {
  const auto l = [...n = args] { (x(n), ...); };
}
```

**Description:** Allows initializing a capture with a pack expansion.

**Documentation:**
[Lambda capture](https://en.cppreference.com/w/cpp/language/lambda#Lambda_capture)

**Notes:**
*** promo
None
***

### Language feature-test macros <sup>[allowed]</sup>

```c++
#if !defined(__cpp_modules) || (__cpp_modules < 201907L)
...  // Toolchain does not support modules
#endif
```

**Description:** Provides a standardized way to test the toolchain's
implementation of a particular language feature.

**Documentation:**
[Feature testing](https://en.cppreference.com/w/cpp/feature_test)

**Notes:**
*** promo
None
***

### [[likely]], [[unlikely]] <sup>[allowed]</sup>

```c++
if (n > 0) [[likely]] {
  return 1;
}
```

**Description:** Tells the optimizer that a particular codepath is more or less
likely than an alternative.

**Documentation:**
[C++ attribute: `likely`, `unlikely`](https://en.cppreference.com/w/cpp/language/attributes/likely)

**Notes:**
*** promo
- [Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/bk9YC5qSDF8)
***

### Range-for statements with initializer <sup>[allowed]</sup>

```c++
T foo();
...
for (auto& x : foo().items()) { ... }                   // UAF before C++23!
for (T thing = foo(); auto& x : thing.items()) { ... }  // OK
```

**Description:** Like C++17's selection statements with initializer.
Particularly useful before C++23, since temporaries inside range-expressions are
not lifetime-extended until the end of the loop before C++23.

**Documentation:**
[Range-based `for` loop](https://en.cppreference.com/w/cpp/language/range-for)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414531)
***

### Three-way comparison ("spaceship") operator <sup>[allowed]</sup>

```c++
// `ordering` is an instance of `std::strong_odering` or `std::partial_ordering`
// that describes how `a` and `b` are related.
const auto ordering = a <=> b;
if (ordering < 0) { ... }       // `a` < `b`
else if (ordering > 0) { ... }  // `a` > `b`
else { ... }                    // `a` == `b`
```

**Description:** Compares two objects in a fashion similar to `strcmp`. Perhaps
most useful when defined as an overload in a class, in which case it can replace
definitions of other inequalities. See also "Default comparisons".

**Documentation:**
[Three-way comparison](https://en.cppreference.com/w/cpp/language/operator_comparison#Three-way_comparison)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414530)
***

## C++20 Allowed Library Features {#library-allowlist-20}

The following C++20 library features are allowed in the Chromium codebase.

### &lt;bit&gt; <sup>[allowed]</sup>

```c++
#include <bit>
```

**Description:** Provides various byte- and bit-twiddling functions, e.g.
counting leading zeros.

**Documentation:**
[Standard library header `<bit>`](https://en.cppreference.com/w/cpp/header/bit)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414634)
***

### &lt;compare&gt; <sup>[allowed]</sup>

```c++
#include <compare>
```

**Description:** Concepts and classes used to implement three-way comparison
("spaceship", `<=>`) support.

**Documentation:**
[Standard library header `<compare>`](https://en.cppreference.com/w/cpp/header/compare)

**Notes:**
*** promo
None
***

### &lt;concepts&gt; <sup>[allowed]</sup>

```c++
#include <concepts>
```

**Description:** Various useful concepts, many of which replace pre-concept
machinery in `<type_traits>`.

**Documentation:**
[Standard library header `<concepts>`](https://en.cppreference.com/w/cpp/header/concepts)

**Notes:**
*** promo
None
***

### Range algorithms <sup>[allowed]</sup>

```c++
constexpr int kArr[] = {2, 4, 6, 8, 10, 12};
constexpr auto is_even = [] (auto x) { return x % 2 == 0; };
static_assert(std::ranges::all_of(kArr, is_even));
```

**Description:** Provides versions of most algorithms that accept either an
iterator-sentinel pair or a single range argument.

**Documentation:**
[Ranges algorithms](https://en.cppreference.com/w/cpp/algorithm/ranges)

**Notes:**
*** promo
Supersedes `//base`'s backports in `//base/ranges/algorithm.h`.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZnIbkfJ0Glw)
***

### Range access, range primitives, dangling iterator handling, and range concepts <sup>[allowed]</sup>

```c++
// Range access:
constexpr int kArr[] = {2, 4, 6, 8, 10, 12};
static_assert(std::ranges::size(kArr) == 6);

// Range primitives:
static_assert(
    std::same_as<std::ranges::iterator_t<decltype(kArr)>, const int*>);

// Range concepts:
static_assert(std::ranges::contiguous_range<decltype(kArr)>);
```

**Description:** Various helper functions and types for working with ranges.

**Documentation:**
[Ranges library](https://en.cppreference.com/w/cpp/ranges)

**Notes:**
*** promo
Supersedes `//base`'s backports in `//base//ranges/ranges.h`.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZnIbkfJ0Glw)
***

### Library feature-test macros and &lt;version&gt; <sup>[allowed]</sup>

```c++
#if !defined(__cpp_lib_atomic_value_initialization) || \
    (__cpp_lib_atomic_value_initialization < 201911L)
...  // `std::atomic` is not value-initialized by default.
#endif
```

**Description:** Provides a standardized way to test the toolchain's
implementation of a particular library feature.

**Documentation:**
[Feature testing](https://en.cppreference.com/w/cpp/feature_test)

**Notes:**
*** promo
None
***

### &lt;numbers&gt; <sup>[allowed]</sup>

```c++
#include <numbers>
```

**Description:** Provides compile-time constants for many common mathematical
values, e.g. pi and e.

**Documentation:**
[Mathematical constants](https://en.cppreference.com/w/cpp/numeric/constants)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414635)
***

### std::assume_aligned <sup>[allowed]</sup>

```c++
void f(int* p) {
  int* aligned = std::assume_aligned<256>(p);
  ...
```

**Description:** Informs the compiler that a pointer points to an address
aligned to at least some particular power of 2.

**Documentation:**
[`std::assume_aligned`](https://en.cppreference.com/w/cpp/memory/assume_aligned)

**Notes:**
*** promo
None
***

### std::erase[_if] for containers <sup>[allowed]</sup>

```c++
std::vector<int> numbers = ...;
std::erase_if(numbers, [](int x) { return x % 2 == 0; });
```

**Description:** Erases from a container by value comparison or predicate,
avoiding the need to use the `erase(remove(...` paradigm.

**Documentation:**
[`std::erase`, `std::erase_if` (`std::vector`)](https://en.cppreference.com/w/cpp/container/vector/erase2)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414639)
***

### std::hardware_{con,de}structive_interference_size <sup>[allowed]</sup>

```c++
struct SharedData {
  ReadOnlyFrequentlyUsed data;
  alignas(std::hardware_destructive_interference_size) std::atomic<size_t> counter;
};
```

**Description:** The `std::hardware_destructive_interference_size` constant is
useful to avoid false sharing (destructive interference) between variables that
would otherwise occupy the same cacheline. In contrast,
`std::hardware_constructive_interference_size` is helpful to promote true
sharing (constructive interference), e.g. to support better locality for
non-contended data.

**Documentation:**
[`std::hardware_destructive_interference_size`](https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size),
[`std::hardware_constructive_interference_size`](https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/cwktrFxxUY4)
***

### std::is_[un]bounded_array <sup>[allowed]</sup>

```c++
template <typename T>
static constexpr bool kBoundedArray = std::is_bounded_array_v<T>;
```

**Description:** Checks if a type is an array type with a known or unknown
bound.

**Documentation:**
[`std::is_bounded_array`](https://en.cppreference.com/w/cpp/types/is_bounded_array),
[`std::is_unbounded_array`](https://en.cppreference.com/w/cpp/types/is_unbounded_array)

**Notes:**
*** promo
None
***

### std::lerp <sup>[allowed]</sup>

```c++
double val = std::lerp(start, end, t);
```

**Description:** Linearly interpolates (or extrapolates) between two values.

**Documentation:**
[`std::lerp`](https://en.cppreference.com/w/cpp/numeric/lerp)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414537)
***

### std::make_obj_using_allocator etc. <sup>[allowed]</sup>

```c++
auto obj = std::make_obj_using_allocator<Obj>(alloc, ...);
```

**Description:** Constructs an object using
[uses-allocator construction](https://en.cppreference.com/w/cpp/memory/uses_allocator).

**Documentation:**
[`std::make_obj_using_allocator`](https://en.cppreference.com/w/cpp/memory/make_obj_using_allocator)

**Notes:**
*** promo
None
***

### std::make_unique_for_overwrite <sup>[allowed]</sup>

```c++
auto ptr = std::make_unique_for_overwrite<int>();  // `*ptr` is uninitialized
```

**Description:** Like calling `std::unique_ptr<T>(new T)` instead of the more
typical `std::unique_ptr<T>(new T(...))`.

**Documentation:**
[`std::make_unique`, `std::make_unique_for_overwrite`](https://en.cppreference.com/w/cpp/memory/unique_ptr/make_unique)

**Notes:**
*** promo
None
***

### std::midpoint <sup>[allowed]</sup>

```c++
int center = std::midpoint(top, bottom);
```

**Description:** Finds the midpoint between its two arguments, avoiding any
possible overflow. For integral inputs, rounds towards the first argument.

**Documentation:**
[`std::midpoint`](https://en.cppreference.com/w/cpp/numeric/midpoint)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414539)
***

### std::remove_cvref[_t] <sup>[allowed]</sup>

```c++
template <typename T,
          typename = std::enable_if_t<std::is_same_v<std::remove_cvref_t<T>,
                                                     int>>>
void foo(T t);
```

**Description:** Provides a way to remove const, volatile, and reference
qualifiers from a type.

**Documentation:**
[`std::remove_cvref`](https://en.cppreference.com/w/cpp/types/remove_cvref)

**Notes:**
*** promo
None
***

### std::ssize <sup>[allowed]</sup>

```c++
str.replace(it, it + std::ssize(substr), 1, 'x');
```

**Description:** Returns the size of an object as a signed type.

**Documentation:**
[`std::size`, `std::ssize`](https://en.cppreference.com/w/cpp/iterator/size)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414543)
***

### std::string::(starts,ends)_with <sup>[allowed]</sup>

```c++
const std::string str = "Foo bar";
const bool is_true = str.ends_with("bar");
```

**Description:** Tests whether a string starts or ends with a particular
character or string.

**Documentation:**
[`std::basic_string<CharT,Traits,Allocator>::starts_with`](https://en.cppreference.com/w/cpp/string/basic_string/starts_with),
[`std::basic_string<CharT,Traits,Allocator>::ends_with`](https://en.cppreference.com/w/cpp/string/basic_string/ends_with)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414647)
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

### std::atomic_ref <sup>[banned]</sup>

```c++
struct S { int a; int b; };
S not_atomic;
std::atomic_ref<S> is_atomic(not_atomic);
```

**Description:** Allows atomic access to objects that might not themselves be
atomic types. While any atomic_ref to an object exists, the object must be
accessed exclusively through atomic_ref instances.

**Documentation:**
[`std::atomic_ref`](https://en.cppreference.com/w/cpp/atomic/atomic_ref)

**Notes:**
*** promo
Banned due to being [unimplemented in libc++](https://reviews.llvm.org/D72240).

[Migration bug](https://crbug.com/1422701) (once this is allowed)
***

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

### Views, range factories, and range adaptors <sup>[banned]</sup>

```c++
constexpr int kArr[] = {6, 2, 8, 4, 4, 2};
constexpr auto plus_one = std::views::transform([](int n){ return n + 1; });
static_assert(std::ranges::equal(kArr | plus_one, {7, 3, 9, 5, 5, 3}));

// Prints 1, 2, 3, 4, 5, 6.
for (auto i : std::ranges::iota_view(1, 7)) {
  std::cout << i << '\n';
}
```

**Description:** Lightweight objects that represent iterable sequences.
Provides facilities for lazy operations on ranges, along with composition into
pipelines.

**Documentation:**
[Ranges library](https://en.cppreference.com/w/cpp/ranges)

**Notes:**
*** promo
Banned in Chrome due to questions about the design, impact on build time, and
runtime performance.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZnIbkfJ0Glw)
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

## C++20 TBD Language Features {#core-review-20}

The following C++20 language features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### Aggregate initialization using parentheses <sup>[tbd]</sup>

```c++
struct B {
  int a;
  int&& r;
} b2(1, 1);  // Warning: dangling reference
```

**Description:** Allows initialization of aggregates using parentheses, not just
braces.

**Documentation:**
[Aggregate initialization](https://en.cppreference.com/w/cpp/language/aggregate_initialization),
[Direct initialization](https://en.cppreference.com/w/cpp/language/direct_initialization)

**Notes:**
*** promo
There are subtle but important differences between brace- and paren-init of
aggregates. The parenthesis style appears to have more pitfalls (allowing
narrowing conversions, not extending lifetimes of temporaries bound to
references).
***

### Coroutines <sup>[tbd]</sup>

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
Requires significant support code and planning around API and migration.

[Prototyping bug](https://crbug.com/1403840)
***

## C++20 TBD Library Features {#library-review-20}

The following C++20 library features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### &lt;coroutine&gt; <sup>[tbd]</sup>

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

### &lt;format&gt; <sup>[tbd]</sup>

```c++
std::cout << std::format("Hello {}!\n", "world");
```

**Description:** Utilities for producing formatted strings.

**Documentation:**
[Formatting library](https://en.cppreference.com/w/cpp/utility/format)

**Notes:**
*** promo
Has both pros and cons compared to `absl::StrFormat` (which we don't yet use).
Migration would be nontrivial.
***

### &lt;source_location&gt; <sup>[tbd]</sup>

```c++
#include <source_location>
```

**Description:** Provides a class that can hold source code details such as
filenames, function names, and line numbers.

**Documentation:**
[Standard library header `<source_location>`](https://en.cppreference.com/w/cpp/header/source_location)

**Notes:**
*** promo
Seems to regress code size vs. `base::Location`.
***

### &lt;span&gt; <sup>[tbd]</sup>

```c++
#include <span>
```

**Description:** Utilities for non-owning views over a sequence of objects.

**Documentation:**
[](https://en.cppreference.com/w/cpp/header/span)

**Notes:**
*** promo
Use `base::span` for now.

[Migration bug](https://crbug.com/1414652)
***

### std::u8string <sup>[tbd]</sup>

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

### bind_front <sup>[banned]</sup>

```c++
absl::bind_front
```

**Description:** Binds the first N arguments of an invocable object and stores
them by value.

**Documentation:**
*   [bind_front.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/bind_front.h)
*   [Avoid std::bind](https://abseil.io/tips/108)

**Notes:**
*** promo
Overlaps with `base::Bind`.
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
Overlaps with `base/ranges/algorithm.h`.
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

### Optional <sup>[banned]</sup>

```c++
absl::optional<int> Func(bool b) {
  return b ? absl::make_optional(1) : abl::nullopt;
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
unnecessary because, in Chromium, it is the same type as `std::string_view`.
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

## Abseil TBD Features {#absl-review}

The following Abseil library features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### AnyInvocable <sup>[tbd]</sup>

```c++
absl::AnyInvocable
```

**Description:** An equivalent of the C++23 std::move_only_function.

**Documentation:**
*   [any_invocable.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/any_invocable.h)
*   [std::move_only_function](https://en.cppreference.com/w/cpp/utility/functional/move_only_function/move_only_function)

**Notes:**
*** promo
Overlaps with `base::RepeatingCallback`, `base::OnceCallback`.
***

### Containers <sup>[tbd]</sup>

```c++
absl::flat_hash_map
absl::flat_hash_set
absl::node_hash_map
absl::node_hash_set
absl::btree_map
absl::btree_set
absl::btree_multimap
absl::btree_multiset
```

**Description:** Alternatives to STL containers designed to be more efficient
in the general case.

**Documentation:**
*   [Containers](https://abseil.io/docs/cpp/guides/container)
*   [Hash](https://abseil.io/docs/cpp/guides/hash)

**Notes:**
*** promo
Supplements `base/containers/`.

absl::InlinedVector is explicitly allowed, see the [discussion
thread](https://groups.google.com/a/chromium.org/g/cxx/c/jTfqVfU-Ka0/m/caaal90NCgAJ).

***

### CRC32C library <sup>[tbd]</sup>

**Description:** API for computing CRC32C values as checksums for arbitrary
sequences of bytes provided as a string buffer.

**Documentation:**
[crc32.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/crc/crc32c.h)

**Notes:**
*** promo
Overlaps with `third_party/crc32c`.
***

### Log macros and related classes <sup>[tbd]</sup>

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
Overlaps with `base/logging.h`.
***

### NoDestructor <sup>[tbd]</sup>

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
[no_destructor.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/base/no_desctructor.h)

**Notes:**
*** promo
Overlaps with `base::NoDestructor`.
***

### Nullability annotations <sup>[tbd]</sup>

```c++
void PaySalary(absl::NotNull<Employee *> employee) {
  pay(*employee);  // OK to dereference
}
```

**Description:** Annotations to more clearly specify contracts

**Documentation:**
[nullability.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/base/nullability.h)

**Notes:**
*** promo
These nullability annotations are primarily a human readable signal about the
intended contract of the pointer. They are not *types* and do not currently
provide any correctness guarantees.
***

### Overload <sup>[tbd]</sup>

```c++
std::variant<int, std::string, double> v(int{1});
assert(std::visit(absl::Overload(
                       [](int) -> absl::string_view { return "int"; },
                       [](const std::string&) -> absl::string_view {
                         return "string";
                       },
                       [](double) -> absl::string_view { return "double"; }),
                    v) == "int");
```

**Description:** Returns a functor that provides overloads based on the functors passed to it

**Documentation:**
[overload.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/overload.h)

**Notes:**
*** promo
Overlaps with `base::Overloaded`.
***
