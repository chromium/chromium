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
*   **C++17:** Initially supported December 23, 2021; see allowed/banned/TBD
    features below
*   **C++20:** _Not yet supported in Chromium_, with the exception of
    [designated initializers](https://google.github.io/styleguide/cppguide.html#Designated_initializers)
*   **C++23:** _Not yet standardized_
*   **Abseil:** _Default allowed; see banned/TBD features below_
      * absl::AnyInvocable: Initially supported June 20, 2022
      * Log library: Initially supported Aug 31, 2022
      * CRC32C library: Initially supported Dec 5, 2022

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
In C++17, structured bindings don't work with lambda captures.
[C++20 will allow capturing structured bindings by value](https://wg21.link/p1091r3).

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
See similar attribute macros in base/compiler_specific.h.
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

### std::in_place[_type,_index][_t] <sup>[banned]</sup>

```c++
std::optional<std::complex<double>> opt{std::in_place, 0, 1};
std::variant<int, float> v{std::in_place_type<int>, 1.4};
```

**Description:** The `std::in_place` are disambiguation tags for
`std::optional`, `std::variant`, and `std::any` to indicate that the object
should be constructed in-place.

**Documentation:**
[`std::in_place`](https://en.cppreference.com/w/cpp/utility/in_place)

**Notes:**
*** promo
Banned for now because `std::optional`, `std::variant`, and `std::any` are all
banned for now. Because `absl::optional` and `absl::variant` are used instead,
and they require `absl::in_place`, use `absl::in_place` for non-Abseil Chromium
code. See the
[discussion thread](https://groups.google.com/a/chromium.org/g/cxx/c/ZspmuJPpv6s).
***

### std::optional <sup>[banned]</sup>

```c++
std::optional<std::string> s;
```

**Description:** The class template `std::optional` manages an optional
contained value, i.e. a value that may or may not be present. A common use case
for optional is the return value of a function that may fail.

**Documentation:**
[`std::optional`](https://en.cppreference.com/w/cpp/utility/optional)

**Notes:**
*** promo
[Will be allowed soon](https://crbug.com/1373619); for now, use
`absl::optional`.
***

### std::[u16]string_view <sup>[banned]</sup>

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
[Will be allowed soon](https://crbug.com/691162); for now, use
`base::StringPiece[16]`, unless interfacing with third-party code, in which
case it is allowed. Note `base::StringPiece[16]` implicitly convert to and from
the corresponding STL types, so one typically does not need to write the STL
name.
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
Use `base::StringPiece` from `base/strings/`, unless interfacing with
third-party code, in which case prefer to write the type as `std::string_view`.
Note `base::StringPiece` implicitly converts to and from `std::string_view`, so
one typically does not need to write the STL name.
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
absl::InlinedVector
absl::FixedArray
```

**Description:** Alternatives to STL containers designed to be more efficient
in the general case.

**Documentation:**
*   [Containers](https://abseil.io/docs/cpp/guides/container)
*   [Hash](https://abseil.io/docs/cpp/guides/hash)

**Notes:**
*** promo
Supplements `base/containers/`.
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
