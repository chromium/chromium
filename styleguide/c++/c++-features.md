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
*   **C++17:** _Initially supported December 23, 2021; see allowed/banned/TBD
    features below_
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
Overlaps with `base/time`. Keep using the `base/time` classes.
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
Use `base::{Once,Repeating}Callback` instead. Compared to `std::function`,
`base::{Once,Repeating}Callback` directly supports Chromium's refcounting
classes and weak pointers and deals with additional thread safety concerns.

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
[Discussion Thread](https://groups.google.com/a/chromium.org/forum/#!topic/cxx/aT2wsBLKvzI)
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
Overlaps with many classes in `base/synchronization`. `base::Thread` is tightly
coupled to `base::MessageLoop` which would make it hard to replace. We should
investigate using standard mutexes, or unique_lock, etc. to replace our
locking/synchronization classes.
***

## C++17 Allowed Language Features {#core-allowlist-17}

The following C++17 language features are allowed in the Chromium codebase.

### Class Template Argument Deduction (CTAD) <sup>[allowed]</sup>

```c++
template <typename T>
struct MyContainer {
  MyContainer(T val) : val{val} {}
  // ...
};
MyContainer c1(1);  // Type deduced to be `int`.
```

**Description:** Automatic template argument deduction much like how it's done
for functions, but now including class constructors.

**Documentation:**
[Class template argument deduction](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction)

**Notes:**
*** promo
Usage is governed by the
[Google Style Guide](https://google.github.io/styleguide/cppguide.html#CTAD).
***

### constexpr if <sup>[allowed]</sup>

```c++
if constexpr (cond) { ...
```

**Description:** Write code that is instantiated depending on a compile-time
condition.

**Documentation:**
[`if` statement](https://en.cppreference.com/w/cpp/language/if)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/op2ePZnjP0w)
***

### constexpr lambda <sup>[allowed]</sup>

```c++
auto identity = [](int n) constexpr { return n; };
static_assert(identity(123) == 123);
```

**Description:** Compile-time lambdas using constexpr.

**Documentation:**
[Lambda expressions](https://en.cppreference.com/w/cpp/language/lambda)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### Declaring non-type template parameters with auto <sup>[allowed]</sup>

```c++
template <auto... seq>
struct my_integer_sequence {
  // ...
};
auto seq = my_integer_sequence<0, 1, 2>();  // Type deduced to be `int`.
```

**Description:** Following the deduction rules of `auto`, while respecting the
non-type template parameter list of allowable types, template arguments can be
deduced from the types of its arguments.

**Documentation:**
[Template parameters](https://en.cppreference.com/w/cpp/language/template_parameters)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### fallthrough attribute <sup>[allowed]</sup>

```c++
case 1:
  DoSomething();
  [[fallthrough]];
case 2:
  break;
```

**Description:**
The `[[fallthrough]]` attribute can be used in switch statements to indicate
when intentionally falling through to the next case.

**Documentation:**
[C++ attribute: `fallthrough`](https://en.cppreference.com/w/cpp/language/attributes/fallthrough)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/JrvyFd243QI)
***

### Fold expressions <sup>[allowed]</sup>

```c++
template <typename... Args>
auto sum(Args... args) {
  return (... + args);
}
```

**Description:** A fold expression performs a fold of a template parameter pack
over a binary operator.

**Documentation:**
[Fold expression](https://en.cppreference.com/w/cpp/language/fold)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/4DTm3idXz0w)
***

### Inline variables <sup>[allowed]</sup>

```c++
struct S {
  static constexpr int kZero = 0;  // constexpr implies inline here.
};

inline constexpr int kOne = 1;  // Explicit inline needed here.
```

**Description:** The `inline` specifier can be applied to variables as well as
to functions. A variable declared inline has the same semantics as a function
declared inline. It can also be used to declare and define a static member
variable, such that it does not need to be initialized in the source file.

**Documentation:**
[`inline` specifier](https://en.cppreference.com/w/cpp/language/inline)

**Notes:**
*** promo
Inline variables in anonymous namespaces in header files will still get one copy
per translation unit, so they must be outside of an anonymous namespace to be
effective.

Mutable inline variables and taking the address of inline variables are banned
since these will break the component build.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/hmyGFD80ocE)
***

### __has_include <sup>[allowed]</sup>

```c++
#if __has_include(<optional>) ...
```

**Description:** Checks whether a file is available for inclusion, i.e. the file
exists.

**Documentation:**
[Source file inclusion](https://en.cppreference.com/w/cpp/preprocessor/include)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### Lambda capture this by value <sup>[allowed]</sup>

```c++
const auto l = [*this] { return member_; }
```

**Description:** `*this` captures the current object by copy, while `this`
continues to capture by reference.

**Documentation:**
[Lambda capture](https://en.cppreference.com/w/cpp/language/lambda#Lambda_capture)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### maybe_unused attribute <sup>[allowed]</sup>

```c++
struct [[maybe_unused]] MyUnusedThing;
[[maybe_unused]] int x;
```

**Description:**
The `[[maybe_unused]]` attribute can be used to indicate that individual
variables, functions, or fields of a class/struct/enum can be left unused.

**Documentation:**
[C++ attribute: `maybe_unused`](https://en.cppreference.com/w/cpp/language/attributes/maybe_unused)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/jPLfU5eRg8M/)
***

### Nested namespaces <sup>[allowed]</sup>

```c++
namespace A::B::C { ...
```

**Description:** Using the namespace resolution operator to create nested
namespace definitions.

**Documentation:**
[Namespaces](https://en.cppreference.com/w/cpp/language/namespace)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/gLdR3apDSmg/)
***

### nodiscard attribute <sup>[allowed]</sup>

```c++
struct [[nodiscard]] ErrorOrValue;
[[nodiscard]] bool DoSomething();
```

**Description:**
The `[[nodiscard]]` attribute can be used to indicate that

  - the return value of a function should not be ignored
  - values of annotated classes/structs/enums returned from functions should not
    be ignored

**Documentation:**
[C++ attribute: `nodiscard`](https://en.cppreference.com/w/cpp/language/attributes/nodiscard)

**Notes:**
*** promo
This replaces the previous `WARN_UNUSED_RESULT` macro, which was a wrapper
around the compiler-specific `__attribute__((warn_unused_result))`.

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/nH7Ar8pZ1Dw)
***

### Selection statements with initializer <sup>[allowed]</sup>

```c++
if (int a = Func(); a < 3) { ...
switch (int a = Func(); a) { ...
```

**Description:** New versions of the if and switch statements which simplify
common code patterns and help users keep scopes tight.

**Documentation:**
[`if` statement](https://en.cppreference.com/w/cpp/language/if),
[`switch` statement](https://en.cppreference.com/w/cpp/language/switch)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/4GP43nftePE)
***

### Structured bindings <sup>[allowed]</sup>

```c++
const auto [x, y] = FuncReturningStdPair();
```

**Description:** Allows writing `auto [x, y, z] = expr;` where the type of
`expr` is a tuple-like object, whose elements are bound to the variables `x`,
`y`, and `z` (which this construct declares). Tuple-like objects include
`std::tuple`, `std::pair`, `std::array`, and aggregate structures.

**Documentation:**
[Structured binding declaration](https://en.cppreference.com/w/cpp/language/structured_binding)
[Explanation of structured binding types](https://jguegant.github.io/blogs/tech/structured-bindings.html)

**Notes:**
*** promo
This feature forces omitting type names. Its use should follow
[the guidance around `auto` in Google C++ Style guide](https://google.github.io/styleguide/cppguide.html#Type_deduction).

[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ExfSorNLNf4)
***

### using declaration for attributes <sup>[allowed]</sup>

```c++
[[using CC: opt(1), debug]]  // same as [[CC:opt(1), CC::debug]]
```

**Description:** Specifies a common namespace for a list of attributes.

**Documentation:**
[Attribute specifier sequence](https://en.cppreference.com/w/cpp/language/attributes)

**Notes:**
*** promo
See similar attribute macros in `base/compiler_specific.h`.
***

## C++17 Allowed Library Features {#library-allowlist-17}

The following C++17 language features are allowed in the Chromium codebase.

### 3D std::hypot <sup>[allowed]</sup>

```c++
double dist = std::hypot(1.0, 2.5, 3.7);
```

**Description:** Computes the distance from the origin in 3D space.

**Documentation:**
[`std::hypot`](https://en.cppreference.com/w/cpp/numeric/math/hypot)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### Searchers <sup>[allowed]</sup>

```c++
auto it = std::search(haystack.begin(), haystack.end(),
                      std::boyer_moore_searcher(needle.begin(), needle.end()));
```

**Description:** Alternate string searching algorithms.

**Documentation:**
[Searchers](https://en.cppreference.com/w/cpp/utility/functional#Searchers)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::apply <sup>[allowed]</sup>

```c++
static_assert(std::apply(std::plus<>(), std::make_tuple(1, 2)) == 3);
```

**Description:** Invokes a `Callable` object with a tuple of arguments.

**Documentation:**
[`std::apply`](https://en.cppreference.com/w/cpp/utility/apply)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/cNZm_g39fyM)
***

### std::as_const <sup>[allowed]</sup>

```c++
auto&& const_ref = std::as_const(mutable_obj);
```

**Description:** Forms reference to const T.

**Documentation:**
[`std::as_const`](https://en.cppreference.com/w/cpp/utility/as_const)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/5Uo4iJK6Mf4)
***

### std::atomic<T>::is_always_lock_free <sup>[allowed]</sup>

```c++
template <typename T>
struct is_lock_free_impl
: std::integral_constant<bool, std::atomic<T>::is_always_lock_free> {};
```

**Description:** True when the given atomic type is always lock-free.

**Documentation:**
[`std::atomic<T>::is_always_lock_free`](https://en.cppreference.com/w/cpp/atomic/atomic/is_always_lock_free)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::clamp <sup>[allowed]</sup>

```c++
int x = std::clamp(inp, 0, 100);
```

**Description:** Clamps a value between a minimum and a maximum.

**Documentation:**
[`std::clamp`](https://en.cppreference.com/w/cpp/algorithm/clamp)

### std::{{con,dis}junction,negation} <sup>[allowed]</sup>

```c++
template<typename T, typename... Ts>
std::enable_if_t<std::conjunction_v<std::is_same<T, Ts>...>>
func(T, Ts...) { ...
```

**Description:** Performs logical operations on type traits.

**Documentation:**
[`std::conjunction`](https://en.cppreference.com/w/cpp/types/conjunction),
[`std::disjunction`](https://en.cppreference.com/w/cpp/types/disjunction),
[`std::negation`](https://en.cppreference.com/w/cpp/types/negation)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/YhlF_sTDSc0)
***

### std::exclusive_scan <sup>[allowed]</sup>

```c++
std::exclusive_scan(data.begin(), data.end(), output.begin());
```

**Description:** Like `std::inclusive_scan` but omits the current element from
the written output at each step; that is, results are "one value behind" those
of `std::inclusive_scan`.

**Documentation:**
[`std::exclusive_scan`](https://en.cppreference.com/w/cpp/algorithm/exclusive_scan)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::gcd <sup>[allowed]</sup>

```c++
static_assert(std::gcd(12, 18) == 6);
```

**Description:** Computes the greatest common divisor of its arguments.

**Documentation:**
[`std::gcd`](https://en.cppreference.com/w/cpp/numeric/gcd)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::has_unique_object_representations <sup>[allowed]</sup>

```c++
std::has_unique_object_representations_v<foo>
```

**Description:** Checks wither the given type is trivially copyable and any two
objects with the same value have the same object representation.

**Documentation:**
[`std::has_unique_object_representations`](https://en.cppreference.com/w/cpp/types/has_unique_object_representations)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::in_place[_t] <sup>[allowed]</sup>

```c++
std::optional<std::complex<double>> opt{std::in_place, 0, 1};
```

**Description:** `std::in_place` is a disambiguation tag for `std::optional` to
indicate that the object should be constructed in-place.

**Documentation:**
[`std::in_place`](https://en.cppreference.com/w/cpp/utility/in_place)

**Notes:**
*** promo
Allowed now that `std::optional` is allowed.
[Migration bug](https://crbug.com/1373619) and
[discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/XG3G85_ZF1k)
***

### std::inclusive_scan <sup>[allowed]</sup>

```c++
std::inclusive_scan(data.begin(), data.end(), output.begin());
```

**Description:** Like `std::accumulate` but writes the result at each step into
the output range.

**Documentation:**
[`std::inclusive_scan`](https://en.cppreference.com/w/cpp/algorithm/inclusive_scan)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::invoke <sup>[allowed]</sup>

```c++
static_assert(std::invoke(std::plus<>(), 1, 2) == 3);
```

**Description:** Invokes a callable object with parameters. A callable object is
e.g. a function, function pointer, functor (that is, an object that provides
`operator()`), lambda, etc.

**Documentation:**
[`std::invoke`](https://en.cppreference.com/w/cpp/utility/functional/invoke)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1412520)
***

### std::is_aggregate <sup>[allowed]</sup>

```c++
if constexpr(std::is_aggregate_v<T>) { ...
```

**Description:** Checks wither the given type is an aggregate type.

**Documentation:**
[`std::is_aggregate`](https://en.cppreference.com/w/cpp/types/is_aggregate)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::is_invocable <sup>[allowed]</sup>

```c++
std::is_invocable_v<Fn, 1, "Hello">
```

**Description:** Checks whether a function may be invoked with the given
argument types.  The `_r` variant also evaluates whether the result is
convertible to a given type.

**Documentation:**
[`std::is_invocable`](https://en.cppreference.com/w/cpp/types/is_invocable)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/YhlF_sTDSc0)
***

### std::is_swappable <sup>[allowed]</sup>

```c++
std::is_swappable<T>
std::is_swappable_with_v<T, U>
```

**Description:** Checks whether classes may be swapped.

**Documentation:**
[`std::is_swappable`](https://en.cppreference.com/w/cpp/types/is_swappable)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::launder <sup>[allowed]</sup>

```c++
struct Y { int z; };
alignas(Y) std::byte s[sizeof(Y)];
Y* q = new(&s) Y{2};
const int h = std::launder(reinterpret_cast<Y*>(&s))->z;
```

**Description:** When used to wrap a pointer, makes it valid to access the
resulting object in cases it otherwise wouldn't have been, in a very limited set
of circumstances.

**Documentation:**
[`std::launder`](https://en.cppreference.com/w/cpp/utility/launder)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::lcm <sup>[allowed]</sup>

```c++
static_assert(std::lcm(12, 18) == 36);
```

**Description:** Computes the least common multiple of its arguments.

**Documentation:**
[`std::lcm`](https://en.cppreference.com/w/cpp/numeric/lcm)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::make_from_tuple <sup>[allowed]</sup>

```c++
// Calls Foo(int, double):
auto foo = std::make_from_tuple<Foo>(std::make_tuple(1, 3.5));
```

**Description:** Constructs an object from a tuple of arguments.

**Documentation:**
[`std::make_from_tuple`](https://en.cppreference.com/w/cpp/utility/make_from_tuple)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::map::{extract,merge} <sup>[allowed]</sup>

```c++
std::map<...>::extract
std::map<...>::merge
std::set<...>::extract
std::set<...>::merge
```

**Description:** Moving nodes and merging containers without the overhead of
expensive copies, moves, or heap allocations/deallocations.

**Documentation:**
[`std::map::extract`](https://en.cppreference.com/w/cpp/container/map/extract),
[`std::map::merge`](https://en.cppreference.com/w/cpp/container/map/merge)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

### std::map::insert_or_assign <sup>[allowed]</sup>

```c++
std::map<std::string, std::string> m;
m.insert_or_assign("c", "cherry");
m.insert_or_assign("c", "clementine");
```

**Description:** Like `operator[]`, but returns more information and does not
require default-constructibility of the mapped type.

**Documentation:**
[`std::map::insert_or_assign`](https://en.cppreference.com/w/cpp/container/map/insert_or_assign)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/Uv2tUfIwUfQ)
***

### std::map::try_emplace <sup>[allowed]</sup>

```c++
std::map<std::string, std::string> m;
m.try_emplace("c", 10, 'c');
m.try_emplace("c", "Won't be inserted");
```

**Description:** Like `emplace`, but does not move from rvalue arguments if the
insertion does not happen.

**Documentation:**
[`std::map::try_emplace`](https://en.cppreference.com/w/cpp/container/map/try_emplace),

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/Uv2tUfIwUfQ)
***

### std::not_fn <sup>[allowed]</sup>

```c++
auto nonwhite = std::find_if(str.begin(), str.end(), std::not_fn(IsWhitespace));
```

**Description:** Creates a forwarding call wrapper that returns the negation of
the callable object it holds.

**Documentation:**
[`std::not_fn`](https://en.cppreference.com/w/cpp/utility/functional/not_fn)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1412529)
***

### std::optional <sup>[allowed]</sup>

```c++
std::optional<std::string> s;
```

**Description:** The class template `std::optional` manages an optional
contained value, i.e. a value that may or may not be present.

**Documentation:**
[`std::optional`](https://en.cppreference.com/w/cpp/utility/optional)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1373619) and
[discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/XG3G85_ZF1k)
***

### std::{size,empty,data} <sup>[allowed]</sup>

```c++
char buffer[260];
memcpy(std::data(buffer), source_str.data(), std::size(buffer));

if (!std::empty(container)) { ... }
```

**Description:** Non-member versions of what are often member functions on STL
containers. Primarily useful when:
- using `std::size()` as a replacement for the old `arraysize()` macro.
- writing code that needs to generically operate across things like
  `std::vector` and `std::list` (which provide `size()`, `empty()`, and `data()
  member functions), `std::array` and `std::initialize_list` (which only provide
  a subset of the aforementioned member functions), and regular arrays (which
  have no member functions at all).

**Documentation:**
[`std::size`](https://en.cppreference.com/w/cpp/iterator/size),
[`std::empty`](https://en.cppreference.com/w/cpp/iterator/empty),
[`std::data`](https://en.cppreference.com/w/cpp/iterator/data)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/58qlA3zk5ZI)

Prefer range-based for loops over `std::size()`: range-based for loops work even
for regular arrays.
***

### std::[u16]string_view <sup>[allowed]</sup>

```c++
std::string_view str = "foo";
std::u16string_view str16 = u"bar";
```

**Description:** A non-owning reference to a string. Useful for providing an
abstraction on top of strings (e.g. for parsing).

**Documentation:**
[`std::basic_string_view`](https://en.cppreference.com/w/cpp/string/basic_string_view)

**Notes:**
*** promo
[Migration bug](https://crbug.com/691162) and
[discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/Ix2BzMzf7WI)
***

### Type trait variable templates <sup>[allowed]</sup>

```c++
bool b = std::is_same_v<int, std::int32_t>;
```

**Description:** Syntactic sugar to provide convenient access to `::value`
members by simply adding `_v`.

**Documentation:**
[Type support](https://en.cppreference.com/w/cpp/types)

**Notes:**
*** promo
[Discussion thread](Non://groups.google.com/a/chromium.org/g/cxx/c/KEa-0AOGRNY)
***

### Uninitialized memory algorithms <sup>[allowed]</sup>

```c++
std::destroy(ptr, ptr + 8);
std::destroy_at(ptr);
std::destroy_n(ptr, 8);
std::uninitialized_move(src.begin(), src.end(), dest.begin());
std::uninitialized_value_construct(std::begin(storage), std::end(storage));
```

**Description:** Replaces direct constructor and destructor calls when manually
managing memory.

**Documentation:**
[`std::destroy`](https://en.cppreference.com/w/cpp/memory/destroy),
[`std::destroy_at`](https://en.cppreference.com/w/cpp/memory/destroy_at),
[`std::destroy_n`](https://en.cppreference.com/w/cpp/memory/destroy_n),
[`std::uninitialized_move`](https://en.cppreference.com/w/cpp/memory/uninitialized_move),
[`std::uninitialized_value_construct`](https://en.cppreference.com/w/cpp/memory/uninitialized_value_construct)

**Notes:**
*** promo
[Discussion thread](https://groups.google.com/u/1/a/chromium.org/g/cxx/c/jNMsxFTd30M)
***

## C++17 Banned Library Features {#library-blocklist-17}

The following C++17 library features are not allowed in the Chromium codebase.

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
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/00cpZ07nye4)

Banned since workaround for lack of RTTI isn't compatible with the component
build ([Bug](https://crbug.com/1096380)). Also see `absl::any`.
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

### std::hardware_{con,de}structive_interference_size <sup>[banned]</sup>

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
Banned for now since these are
[not supported yet](https://github.com/llvm/llvm-project/issues/60174). Allow
once supported.
[Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/cwktrFxxUY4)
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
code. See the
[discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZspmuJPpv6s).
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

## C++17 TBD Language Features {#core-review-17}

The following C++17 language features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### UTF-8 character literals <sup>[tbd]</sup>

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
None
***

## C++17 TBD Library Features {#library-review-17}

The following C++17 library features are not allowed in the Chromium codebase.
See the top of this page on how to propose moving a feature from this list into
the allowed or banned sections.

### Mathematical special functions <sup>[tbd]</sup>

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
May not be supported in libc++, according to the
[library features table](https://en.cppreference.com/w/cpp/17)
***

### Parallel algorithms <sup>[tbd]</sup>

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
May not be supported in libc++, according to the
[library features table](https://en.cppreference.com/w/cpp/17)
***

### std::byte <sup>[tbd]</sup>

```c++
std::byte b = 0xFF;
int i = std::to_integer<int>(b);  // 0xFF
```

**Description:** A standard way of representing data as a byte. `std::byte` is
neither a character type nor an arithmetic type, and the only operator overloads
available are bitwise operations.

**Documentation:**
[`std::byte`](https://en.cppreference.com/w/cpp/types/byte)

**Notes:**
*** promo
No current consensus; see
[discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/bBY0gZa1Otk).
***

### std::{pmr::memory_resource,polymorphic_allocator} <sup>[tbd]</sup>

```c++
#include <memory_resource>
```

**Description:** Manages memory allocations using runtime polymorphism.

**Documentation:**
[`std::pmr::memory_resource`](https://en.cppreference.com/w/cpp/memory/memory_resource),
[`std::pmr::polymorphic_allocator`](https://en.cppreference.com/w/cpp/memory/polymorphic_allocator)

**Notes:**
*** promo
May not be supported in libc++, according to the
[library features table](https://en.cppreference.com/w/cpp/17)
***

### std::reduce <sup>[tbd]</sup>

```c++
std::reduce(std::execution::par, v.cbegin(), v.cend());
```

**Description:** Like `std::accumulate` except the elements of the range may be
grouped and rearranged in arbitrary order.

**Documentation:**
[std::reduce](https://en.cppreference.com/w/cpp/algorithm/reduce)

**Notes:**
*** promo
Makes the most sense in conjunction with `std::execution::par`.
***

### std::timespec_get <sup>[tbd]</sup>

```c++
std::timespec ts;
std::timespec_get(&ts, TIME_UTC);
```

**Description:** Gets the current calendar time in the given time base.

**Documentation:**
[`std::timespec_get`](https://en.cppreference.com/w/cpp/chrono/c/timespec_get)

**Notes:**
*** promo
None
***

### std::{from,to}_chars <sup>[tbd]</sup>

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
None
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
struct S : public T {
  bool operator==(const S&) const = default;  // Compares `T` bases, then `x`,
                                              // then `y`, short-circuiting.
  int x;
  bool y;
};
```

**Description:** Requests that the compiler generate the implementation of any
comparison operator, including `<=>`. Defaulting `<=>` and not declaring `==`
implicitly defaults `==`, which together are sufficient to allow any comparison
as long as callers do not need to take the address of any non-declared operator.

**Documentation:**
[Default comparisons](https://en.cppreference.com/w/cpp/language/default_comparisons)

**Notes:**
*** promo
[Migration bug](https://crbug.com/1414530)
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
[Migration bug](https://crbug.com/1414637)
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
[Migration bug](https://crbug.com/1414646)
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

### std::to_address <sup>[allowed]</sup>

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
[Migration bug](https://crbug.com/1414648)
***

## C++20 Banned Language Features {#core-blocklist-20}

The following C++20 language features are not allowed in the Chromium codebase.

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

[Migration bug](https://crbug.com/1414621)
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

### char8_t <sup>[tbd]</sup>

```c++
char8_t c = u8'x';
```

**Description:** A single UTF-8 code unit. Similar to `unsigned char`, but
considered a distinct type.

**Documentation:**
[Fundamental types](https://en.cppreference.com/w/cpp/language/types#char8_t)

**Notes:**
*** promo
`char8_t*` is not interconvertible with `char*` and many UTF-8 APIs take
`char*`.
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

### [[likely]], [[unlikely]] <sup>[tbd]</sup>

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
[Will be allowed soon](https://crbug.com/1414620); for now, use `[UN]LIKELY`.
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

### &lt;ranges&gt; <sup>[tbd]</sup>

```c++
constexpr int arr[] = {6, 2, 8, 4, 4, 2};
constexpr auto plus_one = std::views::transform([](int n){ return n + 1; });
static_assert(std::ranges::equal(arr | plus_one, {7, 3, 9, 5, 5, 3}));
```

**Description:** Generalizes algorithms using range views, which are lightweight
objects that represent iterable sequences. Provides facilities for eager and
lazy operations on ranges, along with composition into pipelines.

**Documentation:**
[Ranges library](https://en.cppreference.com/w/cpp/ranges)

**Notes:**
*** promo
Significant concerns expressed internally. We should consider whether there are
clearly-safe pieces to allow (e.g. to replace `base/ranges/algorithm.h`) and
engage with the internal library team.
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
Banned since workaround for lack of RTTI isn't compatible with the component
build ([Bug](https://crbug.com/1096380)). Also see `std::any`.
***

### bind_front <sup>[banned]</sup>

```c++
absl::bind_front
```

**Description:** Binds the first N arguments of an invocable object and stores them by value.

**Documentation:**
*   [bind_front.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/functional/bind_front.h)
*   [Avoid std::bind](https://abseil.io/tips/108)

**Notes:**
*** promo
Banned due to overlap with `base::Bind`. Use `base::Bind` instead.
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
Banned since workaround for lack of RTTI isn't compatible with the component
build. ([Bug](https://crbug.com/1096380)) Use `base::CommandLine` instead.
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
Banned due to overlap with `base/ranges/algorithm.h`. Use the `base/ranges/`
facilities instead.
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
  `base::StringPiece`, `base::FunctionRef` is a *non-owning* reference. Using a
  `base::FunctionRef` as a return value or class field is dangerous and likely
  to result in lifetime bugs.
- [Discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/JVN4E4IIYA0)
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
Banned due to overlap with `base::expected`. Use `base::expected` instead.
***

### String Formatting <sup>[banned]</sup>

```c++
absl::StrFormat
```

**Description:** A typesafe replacement for the family of printf() string
formatting routines.

**Documentation:**
[String Formatting](https://abseil.io/docs/cpp/guides/format)

**Notes:**
*** promo
Banned for now due to overlap with `base::StringPrintf()`. See
[migration bug](https://bugs.chromium.org/p/chromium/issues/detail?id=1371963).
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
Banned for now due to overlap with `base/strings`. We
[should re-evalute](https://bugs.chromium.org/p/chromium/issues/detail?id=1371966)
when we've
[migrated](https://bugs.chromium.org/p/chromium/issues/detail?id=691162) from
`base::StringPiece` to `std::string_view`.
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
Banned due to overlap with `base/synchronization/`. We would love
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
Banned due to overlap with `base/time/`. Use `base/time/` instead.
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
Overlaps with //third_party/crc32c.
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
Overlaps and uses same macros names as `base/logging.h`.
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
Overlaps with `base::Overloaded` from `base/functional/overloaded.h`.
***
