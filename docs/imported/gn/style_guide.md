# GN Style Guide

[TOC]

## Naming and ordering within the file

### Location of build files

It usually makes sense to have more build files closer to the code than
fewer ones at the top level; this is in contrast with what we did with
GYP. This makes things easier to find, and also makes the set of owners
required for reviews smaller since changes are more focused to particular
subdirectories.

### Targets

  * Most BUILD files should have a target with the same name as the
    directory. This target should be the first target.
  * Other targets should be in some logical order -- usually
    more important targets will be first, and unit tests will follow the
    corresponding target. If there's no clear ordering, consider
    alphabetical order.
  * Test support libraries should be static libraries named "test\_support".
    For example, "//ui/compositor:test\_support". Test support libraries should
    include as public deps the non-test-support version of the library
    so tests need only depend on the test\_support target (rather than
    both).

Naming advice

  * Targets and configs should be named using lowercase with underscores
    separating words, unless there is a strong reason to do otherwise.
  * Source sets, groups, and static libraries do not need globally unique names.
    Prefer to give such targets short, non-redundant names without worrying
    about global uniqueness. For example, it looks much better to write a
    dependency as `"//mojo/public/bindings"` rather than
    `"//mojo/public/bindings:mojo_bindings"`
  * Shared libraries (and by extension, components) must have globally unique
    output names. Give such targets short non-unique names above, and then
    provide a globally unique `output_name` for that target.
  * Executables and tests should be given a globally unique name. Technically
    only the output names must be unique, but since only the output names
    appear in the shell and on bots, it's much less confusing if the name
    matches the other places the executable appears.

### Configs

  * A config associated with a single target should be named the same as
    the target with `_config` following it.
  * A config should appear immediately before the corresponding target
    that uses it.

### Example

Example for the `src/foo/BUILD.gn` file:

```
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Config for foo is named foo_config and immediately precedes it in the file.
config("foo_config") {
}

# Target matching path name is the first target.
executable("foo") {
}

# Test for foo follows it.
test("foo_unittests") {
}

config("bar_config") {
}

source_set("bar") {
}
```

## Ordering within a target

  1. `output_name` / `visibility` / `testonly`
  2. `sources`
  3. `cflags`, `include_dirs`, `defines`, `configs` etc. in whatever
     order makes sense to you.
  4. `public_deps`
  5. `deps`

### Conditions

Simple conditions affecting just one variable (e.g. adding a single
source or adding a flag for one particular OS) can go beneath the
variable they affect. More complicated conditions affecting more than
one thing should go at the bottom.

Conditions should be written to minimize the number of conditional blocks.

## Formatting and indenting

GN contains a built-in code formatter which defines the formatting style.
Some additional notes:

  * Variables are `lower_case_with_underscores`.
  * Comments should be complete sentences with periods at the end.
  * Compiler flags and such should always be commented with what they do
    and why the flag is needed.

### Sources

Prefer to list sources only once. It is OK to conditionally include sources
rather than listing them all at the top and then conditionally excluding them
when they don't apply. Conditional inclusion is often clearer since a file is
only listed once and it's easier to reason about when reading.

```
  sources = [
    "main.cc",
  ]
  if (use_aura) {
    sources += [ "thing_aura.cc" ]
  }
  if (use_gtk) {
    sources += [ "thing_gtk.cc" ]
  }
```

### Deps

  * Deps should be in alphabetical order.
  * Deps within the current file should be written first and not
    qualified with the file name (just `:foo`).
  * Other deps should always use fully-qualified path names unless
    relative ones are required for some reason.
  * Prefer to omit the colon and name when possible. See the [GN implicit
    names](reference.md#implicit-names).

```
  deps = [
    ":a_thing",
    ":mystatic",
    "//foo/bar:other_thing",
    "//foo/baz:that_thing",
  ]
```

### Import

Use fully-qualified paths for imports:

```
import("//foo/bar/baz.gni")  # Even if this file is in the foo/bar directory
```

## Usage

### Source sets versus static libraries

Source sets and static libraries can be used interchangeably in most cases. If
you're unsure what to use, a source set is almost never wrong and is less likely
to cause problems, but on a large project using the right kind of target can
be important, so you should know about the following tradeoffs.

Static libraries follow different linking rules. When a static library is
included in a link, only the object files that contain unresolved symbols will
be brought into the build. Source sets result in every object file being added
to the link line of the final binary.

  * If you're eventually linking code into a component, shared library, or
    loadable module, you normally need to use source sets. This is because
    object files with no symbols referenced from within the shared library will
    not be linked into the final library at all. This omission will happen even
    if that object file has a symbol marked for export that targets dependent
    on that shared library need. This will result in undefined symbols when
    linking later targets.

  * Unit tests (and anything else with static initializers with side effects)
    must use source sets. The gtest TEST macros create static initializers
    that register the test. But since no code references symbols in the object
    file, linking a test into a static library and then into a test executable
    means the tests will get stripped.

  * On some platforms, static libraries may involve duplicating all of the
    data in the object files that comprise it. This takes more disk space and
    for certain very large libraries in configurations with very large object
    files can cause internal limits on the size of static libraries to be
    exceeded. Source sets do not have this limitation. Some targets switch
    between source sets and static libraries depending on the build
    configuration to avoid this problem. Some platforms (or toolchains) may
    support something called "thin archives" which don't have this problem;
    but you can't rely on this as a portable solution.

  * Source sets can have no sources, while static libraries will give strange
    platform-specific errors if they have no sources. If a target has only
    headers (for include checking purposes) or conditionally has no sources on
    some platforms, use a source set.

  * In cases where a lot of the symbols are not needed for a particular link
    (this especially happens when linking test binaries), putting that code in
    a static library can dramatically increase linking performance. This is
    because the object files not needed for the link are never considered in
    the first place, rather than forcing the linker to strip the unused code
    in a later pass when nothing references it.

### Components versus shared libraries versus source sets

A component is a Chrome template (rather than a built-in GN concept) that
expands either to a shared library or a static library / source set depending
on the value of the `is_component_build` variable. This allows release builds
to be linked statically in a large binary, but for developers to use shared
libraries for most operations. Chrome developers should almost always use
a component instead of shared library directly.

Much like the source set versus static library tradeoff, there's no hard
and fast rule as to when you should use a component or not. Using
components can significantly speed up incremental builds by making
linking much faster, but they require you to have to think about which
symbols need to be exported from the target.

### Loadable modules versus shared libraries

A shared library will be listed on the link line of dependent targets and will
be loaded automatically by the operating system when the application starts
and symbols automatically resolved. A loadable module will not be linked
directly and the application must manually load it.

On Windows and Linux shared libraries and loadable modules result in the same
type of file (`.dll` and `.so`, respectively). The only difference is in how
they are linked to dependent targets. On these platforms, having a `deps`
dependency on a loadable module is the same as having a `data_deps`
(non-linked) dependency on a shared library.

On Mac, these targets have different formats: a shared library will generate a
`.dylib` file and a loadable module will generate a `.so` file.

Use loadable modules for things like plugins. In the case of plugin-like
libraries, it's good practice to use both a loadable module for the target type
(even for platforms where it doesn't matter) and data deps for targets that
depend on it so it's clear from both places that how the library will be linked
and loaded.

## Build arguments

### Scope

Build arguments should be scoped to a unit of behavior, e.g. enabling a feature.
Typically an argument would be declared in an imported file to share it with
the subset of the build that could make use of it.

Chrome has many legacy flags in `//build/config/features.gni`,
`//build/config/ui.gni`. These locations are deprecated. Feature flags should
go along with the code for the feature. Many browser-level features can go
somewhere in `//chrome/` without lower-level code knowing about it. Some
UI environment flags can go into `//ui/`, and many flags can also go with
the corresponding code in `//components/`. You can write a `.gni` file in
components and have build files in chrome or content import it if necessary.

The way to think about things in the `//build` directory is that this is
DEPSed into various projects like V8 and WebRTC. Build flags specific to
code outside of the build directory shouldn't be in the build directory, and
V8 shouldn't get feature defines for Chrome features.

New feature defines should use the buildflag system. See
`//build/buildflag_header.gni` which allows preprocessor defines to be
modularized without many of the disadvantages that made us use global defines
in the past.

### Type

Arguments support all the [GN language types](language.md#Language).

In the vast majority of cases `boolean` is the preferred type, since most
arguments are enabling or disabling features or includes.

`String`s are typically used for filepaths. They are also used for enumerated
types, though `integer`s are sometimes used as well.

### Naming conventions

While there are no hard and fast rules around argument naming there are
many common conventions. If you ever want to see the current list of argument
names and default values for your current checkout use
`gn args out/Debug --list --short`.

`use_foo` - indicates dependencies or major codepaths to include (e.g.
`use_open_ssl`, `use_ozone`, `use_cups`)

`enable_foo` - indicates feature or tools to be enabled (e.g.
`enable_google_now`, `enable_nacl`, `enable_remoting`, `enable_pdf`)

`disable_foo` - _NOT_ recommended, use `enable_foo` instead with swapped default
value

`is_foo` - usually a global state descriptor (e.g. `is_chrome_branded`,
`is_desktop_linux`); poor choice for non-globals

`foo_use_bar` - prefixes can be used to indicate a limited scope for an argument
(e.g. `rtc_use_h264`, `v8_use_snapshot`)

#### Variables

Prefix top-level local variables within `.gni` files with an underscore. This
prefix causes variables to be unavailable to importing scripts.

```
_this_var_will_not_be_exported = 1
but_this_one_will = 2
```
