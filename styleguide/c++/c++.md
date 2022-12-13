# Chromium C++ style guide

_For other languages, please see the
[Chromium style guides](https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md)._

Chromium follows the [Google C++ Style
Guide](https://google.github.io/styleguide/cppguide.html) unless an exception
is listed below.

A checkout should give you
[clang-format](https://chromium.googlesource.com/chromium/src/+/main/docs/clang_format.md)
to automatically format C++ code. By policy, Clang's formatting of code should
always be accepted in code reviews.

You can propose changes to this style guide by sending an email to
`cxx@chromium.org`. Ideally, the list will arrive at some consensus and you can
request review for a change to this file. If there's no consensus,
`src/styleguide/c++/OWNERS` get to decide.

Blink code in `third_party/blink` uses [Blink style](blink-c++.md).

## Modern C++ features

Google and Chromium style
[targets C++17](https://google.github.io/styleguide/cppguide.html#C++_Version).
Additionally, some features of supported C++ versions remain forbidden. The
status of Chromium's C++ support is covered in more detail in
[Modern C++ use in Chromium](c++-features.md).

## Naming

  * "Chromium" is the name of the project, not the product, and should never
    appear in code, variable names, API names etc. Use "Chrome" instead.

## Tests and Test-only Code

  * Functions used only for testing should be restricted to test-only usages
    with the testing suffixes supported by
    [PRESUBMIT.py](https://chromium.googlesource.com/chromium/src/+/main/PRESUBMIT.py).
    `ForTesting` is the conventional suffix although similar patterns, such as
    `ForTest`, are also accepted. These suffixes are checked at presubmit time
    to ensure the functions are called only by test files.
  * Classes used only for testing should be in a GN build target that is
    marked `testonly=true`. Tests can depend on such targets, but production
    code can not.
  * While test files generally appear alongside the production code they test,
    support code for `testonly` targets should be placed in a `test/` subdirectory.
    For example, see `//mojo/core/core_unittest.cc` and
    `//mojo/core/test/mojo_test_base.cc`. For test classes used across multiple
    directories, it might make sense to move them into a nested `test` namespace for
    clarity.
  * Despite the Google C++ style guide
    [deprecating](https://google.github.io/styleguide/cppguide.html#File_Names)
    the `_unittest.cc` suffix for unit test files, in Chromium we still use this
    suffix to distinguish unit tests from browser tests, which are written in
    files with the `_browsertest.cc` suffix.

## Code formatting

  * Put `*` and `&` by the type rather than the variable name.
  * In class declarations, group function overrides together within each access
    control section, with one labeled group per parent class.
  * Prefer `(foo == 0)` to `(0 == foo)`.

## Unnamed namespaces

Items local to a .cc file should be wrapped in an unnamed namespace. While some
such items are already file-scope by default in C++, not all are; also, shared
objects on Linux builds export all symbols, so unnamed namespaces (which
restrict these symbols to the compilation unit) improve function call cost and
reduce the size of entry point tables.

## Exporting symbols

Symbols can be exported (made visible outside of a shared library/DLL) by
annotating with a `<COMPONENT>_EXPORT` macro name (where `<COMPONENT>` is the
name of the component being built, e.g. BASE, NET, CONTENT, etc.). Class
annotations should precede the class name:
```c++
class FOO_EXPORT Foo {
  void Bar();
  void Baz();
  // ...
};
```

Function annotations should precede the return type:
```c++
class FooSingleton {
  FOO_EXPORT Foo& GetFoo();
  FOO_EXPORT Foo& SetFooForTesting(Foo* foo);
  void SetFoo(Foo* foo);  // Not exported.
};
```

## Multiple inheritance

Multiple inheritance and virtual inheritance are permitted in Chromium code,
but discouraged (beyond the "interface" style of inheritance allowed by the
Google style guide, for which we do not require classes to have the "Interface"
suffix). Consider whether composition could solve the problem instead.

## Inline functions

Simple accessors should generally be the only inline functions. These should be
named using `snake_case()`. Virtual functions should never be declared this way.

## Logging

Remove most logging calls before checking in. Unless you're adding temporary
logging to track down a specific bug, and you have a plan for how to collect
the logged data from user machines, you should generally not add logging
statements.

For the rare case when logging needs to stay in the codebase for a while,
prefer `DVLOG(1)` to other logging methods. This avoids bloating the release
executable and in debug can be selectively enabled at runtime by command-line
arguments:
  * `--v=n` sets the global log level to n (default 0). All log statements with
    a log level less than or equal to the global level will be printed.
  * `--vmodule=mod=n[,mod=n,...]` overrides the global log level for the module
    mod. Supplying the string foo for mod will affect all files named foo.cc,
    while supplying a wildcard like `*bar/baz*` will affect all files with
    `bar/baz` in their full pathnames.

## Platform-specific code

To `#ifdef` code for specific platforms, use the macros defined in
`build/build_config.h` and in the Chromium build config files, not other macros
set by specific compilers or build environments (e.g. `WIN32`).

Place platform-specific #includes in their own section below the "normal"
`#includes`. Repeat the standard `#include` order within this section:

```c++
  #include "foo/foo.h"

  #include <stdint.h>
  #include <algorithm>

  #include "base/strings/utf_string_conversions.h"
  #include "build/build_config.h"
  #include "chrome/common/render_messages.h"

  #if BUILDFLAG(IS_WIN)
  #include <windows.h>
  #include "base/win/com_init_util.h"
  #elif BUILDFLAG(IS_POSIX)
  #include "base/posix/global_descriptors.h"
  #endif
```

## Types

  * Refer to the [Mojo style
    guide](https://chromium.googlesource.com/chromium/src/+/main/docs/security/mojo.md)
    when working with types that will be passed across network or process
    boundaries. For example, explicitly-sized integral types must be used for
    safety, since the sending and receiving ends may not have been compiled
    with the same sizes for things like `int` and `size_t`.
  * Use `size_t` for object and allocation sizes, object counts, array and
    pointer offsets, vector indices, and so on. This prevents casts when
    dealing with STL APIs, and if followed consistently across the codebase,
    minimizes casts elsewhere.
  * Occasionally classes may have a good reason to use a type other than
    `size_t` for one of these concepts, e.g. as a storage space optimization. In
    these cases, continue to use `size_t` in public-facing function
    declarations, and continue to use unsigned types internally (e.g.
    `uint32_t`).
  * Follow the [integer semantics
    guide](https://chromium.googlesource.com/chromium/src/+/main/docs/security/integer-semantics.md)
    for all arithmetic conversions and calculations used in memory management
    or passed across network or process boundaries. In other circumstances,
    follow [Google C++ casting
    conventions](https://google.github.io/styleguide/cppguide.html#Casting)
    to convert arithmetic types when you know the conversion is safe. Use
    `checked_cast<T>` (from `base/numerics/safe_conversions.h`) when you need to
    `CHECK` that the source value is in range for the destination type. Use
    `saturated_cast<T>` if you instead wish to clamp out-of-range values.
    `CheckedNumeric` is an ergonomic way to perform safe arithmetic and casting
    in many cases.
  * The Google Style Guide [bans
    UTF-16](https://google.github.io/styleguide/cppguide.html#Non-ASCII_Characters).
    For various reasons, Chromium uses UTF-16 extensively. Use `std::u16string`
    and `char16_t*` for 16-bit strings, `u"..."` to declare UTF-16 literals, and
    either the actual characters or the `\uXXXX` or `\UXXXXXXXX` escapes for
    Unicode characters. Avoid `\xXX...`-style escapes, which can cause subtle
    problems if someone attempts to change the type of string that holds the
    literal. In code used only on Windows, it may be necessary to use
    `std::wstring` and `wchar_t*`; these are legal, but note that they are
    distinct types and are often not 16-bit on other platforms.

## Object ownership and calling conventions

When functions need to take raw or smart pointers as parameters, use the
following conventions. Here we refer to the parameter type as `T` and name as
`t`.
  * If the function does not modify `t`'s ownership, declare the param as `T*`.
    The caller is expected to ensure `t` stays alive as long as necessary,
    generally through the duration of the call. Exception: In rare cases (e.g.
    using lambdas with STL algorithms over containers of `unique_ptr<>`s), you
    may be forced to declare the param as `const std::unique_ptr<T>&`. Do this
    only when required.
  * If the function takes ownership of a non-refcounted object, declare the
    param as `std::unique_ptr<T>`.
  * If the function (at least sometimes) takes a ref on a refcounted object,
    declare the param as `scoped_refptr<T>`. The caller can decide
    whether it wishes to transfer ownership (by calling `std::move(t)` when
    passing `t`) or retain its ref (by simply passing t directly).
  * In short, functions should never take ownership of parameters passed as raw
    pointers, and there should rarely be a need to pass smart pointers by const
    ref.

Conventions for return values are similar with an important distinction:
  * Return raw pointers if-and-only-if the caller does not take ownership.
  * Return `std::unique_ptr<T>` or `scoped_refptr<T>` by value when the impl is
    handing off ownership.
  * **Distinction**: Return `const scoped_refptr<T>&` when the impl retains
    ownership so the caller isn't required to take a ref: this avoids bumping
    the reference count if the caller doesn't need ownership and also
    [helps binary size](https://crrev.com/c/1435627)).

A great deal of Chromium code predates the above rules. In particular, some
functions take ownership of params passed as `T*`, or take `const
scoped_refptr<T>&` instead of `T*`, or return `T*` instead of
`scoped_refptr<T>` (to avoid refcount churn pre-C++11). Try to clean up such
code when you find it, or at least not make such usage any more widespread.

## Non-owning pointers in class fields

Use `const raw_ref<T>` or `raw_ptr<T>` for class and struct fields in place of a
raw C++ reference `T&` or pointer `T*` whenever possible, except in paths that include
`/renderer/` or `blink/public/web/`.  These a non-owning smart pointers that
have improved memory-safety over raw pointers and references, and can prevent
exploitation of a significant percentage of Use-after-Free bugs.

Prefer `const raw_ref<T>` whenever the held pointer will never be null, and it's
ok to drop the `const` if the internal reference can be reassigned to point to a
different `T`. Use `raw_ptr<T>` in order to express that the pointer _can_ be
null. Only `raw_ptr<T>` can be default-constructed, since `raw_ref<T>` disallows
nullness.

Using `raw_ref<T>` or `raw_ptr<T>` may not be possible in rare cases for
[performance reasons](../../base/memory/raw_ptr.md#Performance). Additionally,
`raw_ptr<T>` doesn’t support some C++ scenarios (e.g. `constexpr`, ObjC
pointers).  Tooling will help to encourage use of these types in the future. See
[raw_ptr.md](../../base/memory/raw_ptr.md#When-to-use-raw_ptr_T) for how to add
exclusions.

## Forward declarations vs. #includes

Unlike the Google style guide, Chromium style prefers forward declarations to
`#includes` where possible. This can reduce compile times and result in fewer
files needing recompilation when a header changes.

You can and should use forward declarations for most types passed or returned
by value, reference, or pointer, or types stored as pointer members or in most
STL containers. However, if it would otherwise make sense to use a type as a
member by-value, don't convert it to a pointer just to be able to
forward-declare the type.

## File headers

All files in Chromium start with a common license header. That header should
look like this:

```c++
// Copyright $YEAR The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
```

Some important notes about this header:
  * `$YEAR` should be set to the current year at the time a file is created, and
    not changed thereafter.
  * For files specific to ChromiumOS, replace the word Chromium with the phrase
    ChromiumOS.
  * The Chromium project hosts mirrors of some upstream open-source projects.
    When contributing to these portions of the repository, retain the existing
    file headers.

Use standard `#include` guards in all header files (see the Google style guide
sections on these for the naming convention). Do not use `#pragma once`;
historically it was not supported on all platforms, and it does not seem to
outperform #include guards even on platforms which do support it.

## CHECK(), DCHECK(), and NOTREACHED()

The `CHECK()` macro will cause an immediate crash if its condition is not met.
`DCHECK()` is like `CHECK()` but is only compiled in when `DCHECK_IS_ON` is true
(debug builds and some bot configurations, but not end-user builds).
`NOTREACHED()` is equivalent to `DCHECK(false)`. Here are some rules for using
these:
  * Use `DCHECK()` or `NOTREACHED()` as assertions, e.g. to document pre- and
    post-conditions. A `DCHECK()` means "this condition must always be true",
    not "this condition is normally true, but perhaps not in exceptional
    cases." Things like disk corruption or strange network errors are examples
    of exceptional circumstances that nevertheless should not result in
    `DCHECK()` failure.
  * A consequence of this is that you should not handle DCHECK() failures, even
    if failure would result in a crash. Attempting to handle a `DCHECK()`
    failure is a statement that the `DCHECK()` can fail, which contradicts the
    point of writing the `DCHECK()`. In particular, do not write code like the
    following:
    ```c++
      DCHECK(foo);
      if (!foo) {  // Eliminate this code.
        ...
      }

      if (!bar) {  // Replace this whole conditional with "DCHECK(bar);".
        NOTREACHED();
        return;
      }
    ```
  * Use `CHECK()` if the consequence of a failed assertion would be a security
    vulnerability, where crashing the browser is preferable. Because this takes
    down the whole browser, sometimes there are better options than `CHECK()`.
    For example, if a renderer sends the browser process a malformed IPC, an
    attacker may control the renderer, but we can simply kill the offending
    renderer instead of crashing the whole browser.
  * You can temporarily use `CHECK()` instead of `DCHECK()` when trying to
    force crashes in release builds to sniff out which of your assertions is
    failing. Don't leave these in the codebase forever; remove them or change
    them back once you've solved the problem.
  * Don't use these macros in tests, as they crash the test binary and leave
    bots in a bad state. Use the `ASSERT_xx()` and `EXPECT_xx()` family of
    macros, which report failures gracefully and can continue running other
    tests.
  * Dereferencing a null pointer in C++ is generally UB (undefined behavior) as
    the compiler is free to assume a dereference means the pointer is not null
    and may apply optimizations based on that. As such, there is sometimes a
    strong opinion to `CHECK()` pointers before dereference. Chromium builds
    with the `no-delete-null-pointer-checks` Clang/GCC flag which prevents such
    optimizations, meaning the side effect of a null dereference would just be
    the use of 0x0 which will lead to a crash on all the platforms Chromium
    supports. As such we do not use `CHECK()` to guard pointer deferences. A
    `DCHECK()` can be used to document that a pointer is never null, and doing
    so as early as possible can help with debugging, though our styleguide now
    recommends using a reference instead of a pointer when it cannot be null.

## Test-only code paths in production code

Try to avoid test-only code paths in production code. Such code paths make
production code behave differently in tests. This makes both tests and
production code hard to reason about. Consider dependency injection, fake
classes, etc to avoid such code paths.

However, if a test-only path in production code cannot be avoided, instrument
that code path with `CHECK_IS_TEST();` to assert that the code is only run in
tests.

```c++
// `profile_manager` may not be available in tests.
if (!profile_manager) {
  CHECK_IS_TEST();
  return std::string();
}
```

`CHECK_IS_TEST();` will crash outside of tests. This asserts that the test-only
code path is not accidentally or maliciously taken in production.

## Miscellany

  * Use UTF-8 file encodings and LF line endings.
  * Unit tests and performance tests should be placed in the same directory as
    the functionality they're testing.
  * The [C++ Dos and Don'ts](c++-dos-and-donts.md) page has more helpful
    information.
