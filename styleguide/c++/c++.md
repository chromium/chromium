# Chromium C++ style guide

_For other languages, please see the [Chromium style guides](https://chromium.googlesource.com/chromium/src/+/master/styleguide/styleguide.md)._

Chromium follows the [Google C++ Style
Guide](https://google.github.io/styleguide/cppguide.html) unless an exception
is listed below.

A checkout should give you
[clang-format](https://chromium.googlesource.com/chromium/src/+/master/docs/clang_format.md)
to automatically format C++ code. By policy, Clang's formatting of code should
always be accepted in code reviews.

You can propose changes to this style guide by sending an email to
`cxx@chromium.org`. Ideally, the list will arrive at some consensus and you can
request review for a change to this file. If there's no consensus,
`src/styleguide/c++/OWNERS` get to decide.

Blink code in `third_party/WebKit` uses [Blink style](blink-c++.md).

## Modern C++ features

Some features of C++ remain forbidden, even as Chromium adopts newer versions
of the C++ language and standard library. These should be similar to those
allowed in Google style, but may occasionally differ. The status of modern C++
features in Chromium is tracked in the separate
[C++ use in Chromium](https://chromium-cpp.appspot.com/) page.

## Naming

  * "Chromium" is the name of the project, not the product, and should never
    appear in code, variable names, API names etc. Use "Chrome" instead.

## Test-only Code

  * Functions used only for testing should be restricted to test-only usages
    with the `ForTesting` suffix. This is checked at presubmit time to ensure
    these functions are only called by test files.

## Code formatting

  * Put `*` and `&` by the type rather than the variable name.
  * In class declarations, group function overrides together within each access
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
  #include "chrome/common/render_messages.h"

  #if defined(OS_WIN)
  #include <windows.h>
  #include "base/win/com_init_util.h"
  #elif defined(OS_POSIX)
  #include "base/posix/global_descriptors.h"
  #endif
```

## Types

  * Use `size_t` for object and allocation sizes, object counts, array and
    pointer offsets, vector indices, and so on. This prevents casts when
    dealing with STL APIs, and if followed consistently across the codebase,
    minimizes casts elsewhere.
  * Occasionally classes may have a good reason to use a type other than
    `size_t` for one of these concepts, e.g. as a storage space optimization. In
    these cases, continue to use `size_t` in public-facing function
    declarations, and continue to use unsigned types internally (e.g.
    `uint32_t`).
  * Follow [Google C++ casting
    conventions](https://google.github.io/styleguide/cppguide.html#Casting)
    to convert arithmetic types when you know the conversion is safe. Use
    `checked_cast<T>` (from `base/numerics/safe_conversions.h`) when you need to
    `CHECK` that the source value is in range for the destination type. Use
    `saturated_cast<T>` if you instead wish to clamp out-of-range values.
    `CheckedNumeric` is an ergonomic way to perform safe arithmetic and casting
    in many cases.
  * When passing values across network or process boundaries, use
    explicitly-sized types for safety, since the sending and receiving ends may
    not have been compiled with the same sizes for things like `int` and
    `size_t`. However, to the greatest degree possible, avoid letting these
    sized types bleed through the APIs of the layers in question.
  * Don't use `std::wstring`. Use `base::string16` or `base::FilePath` instead.
    (Windows-specific code interfacing with system APIs using `wstring` and
    `wchar_t` can still use `string16` and `char16`; it is safe to assume that
    these are equivalent to the "wide" types.)

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
// Copyright $YEAR The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
```

Some important notes about this header:
  * There is no `(c)` after `Copyright`.
  * `$YEAR` should be set to the current year at the time a file is created, and
    not changed thereafter.
  * For files specific to Chromium OS, replace the word Chromium with the phrase
    Chromium OS.
  * If the style changes, don't bother to update existing files to comply with
    the new style. For the same reason, don't just blindly copy an existing
    file's header when creating a new file, since the existing file may use an
    outdated style.
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
      if (!foo)  // Eliminate this code.
        ...

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

## Miscellany

  * Use UTF-8 file encodings and LF line endings.
  * Unit tests and performance tests should be placed in the same directory as
    the functionality they're testing.
  * The [C++ Dos and Don'ts](c++-dos-and-donts.md) page has more helpful
    information.
