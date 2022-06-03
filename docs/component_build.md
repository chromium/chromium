# The Chrome Component Build

## Introduction

Release builds are “static” builds which compile to one executable and
zero-to-two shared libraries (depending on the platform). This is efficient at
runtime, but can take a long time to link because so much code goes into a
single binary.

In a component build, many smaller shared libraries will be generated. This
speeds up link times, and means that many changes only require that the local
shared library be linked rather than the full executable, but at the expense of
program load-time performance.

The component build is currently the default for debug non-iOS builds (it
doesn’t work for iOS). You can force it on for release builds using the
[GN build arg](https://www.chromium.org/developers/gn-build-configuration):

```python
is_component_build = true
```

### How to make a component

Defining a component just means using the GN “component” template instead
of a shared library, static library, or source set. The template will
generate a shared library when `is_component_build` is enabled, and a static
library otherwise.

```python
component("browser") {
  output_name = "chrome_browser"
  sources = ...
  ...
}
```

Shared libraries in GN must have globally unique output names. According to GN
style, your target should be named something simple and convenient (often
matching your directory name). If this is non-unique, override it with the
`output_name` variable.

**Note**: for information about `mojom_component` targets, see
[Mojo documentation](/mojo/public/tools/bindings/README.md#Component-targets).

### Dependencies between targets

When a component directly or indirectly depends on a static library or source
set, it will be linked into this component. If other components do the same,
the static library or source set’s code will be duplicated.

In a few cases (for defining some constants) this duplication is OK, but in
general this is a bad idea. Globals and singletons will get duplicated which
will wreak havoc. Therefore, you should normally ensure that components only
depend on other components.

### Component granularity

Creating lots of small components isn’t desirable. Some code can easily get
duplicated, it takes extra time to create the shared libraries themselves, load
time will get worse, and the build and code can get complicated. On the other
extreme, very large components negate the benefits of the component build. A
good rule of thumb is that components should be medium sized, somewhere in the
range of several dozen to several hundred files.

## Exporting and importing symbols

When a shared library or executable uses a symbol from a shared library, it is
“imported” by the user of the symbol, and “exported” from the shared library
that defines the symbol. Don’t confuse exported symbols with the public API of
a component. For example, unit tests will often require implementation details
to be exported. Export symbols to make the build link the way you need it, and
use GN’s public headers and visibility restrictions to define your public API.

Component library headers can use the `COMPONENT_EXPORT()` macro defined in
`base/component_export.h` to annotate symbols which should be exported by
the component. This macro takes a globally unique component name as an
argument:

```c++
#include "base/component_export.h"

class COMPONENT_EXPORT(YOUR_COMPONENT) YourClass { ... };

COMPONENT_EXPORT(YOUR_COMPONENT) void SomeFunction();
```

When defining the target for your component, set:

```python
defines = [ "IS_YOUR_COMPONENT_IMPL" ]
```

This ensures that the corresponding `COMPONENT_EXPORT(YOUR_COMPONENT)`
invocations result in symbols being marked for export when compiling the
component target. All other targets which include the component's headers
will not have defined `IS_YOUR_COMPONENT_IMPL`, so they will have the same
symbols marked for import instead.

## Chrome’s deprecated pattern for exports

**NOTE**: This section is included for posterity, as many components in the tree
still use this pattern for exports. New components should use
`base/component_export.h` as described above.

Write a header with the name `<component_name>_export.h`. Copy an [existing
one](https://cs.chromium.org/chromium/src/ipc/ipc_export.h)
and update the macro names. It will key off of two macros:

  * `COMPONENT_BUILD`: A globally defined preprocessor definition set when the
    component build is on.
  * `<component_name>_IMPLEMENTATION`: A macro you define for code inside your
    component, and leave undefined for code outside of your component. The
    naming should match your `*_export.h` header.

It will define a macro `<component_name>_EXPORT`. This will use the
`*_IMPLEMENTATION` macro to know whether code is being compiled inside or outside
of your component, and the `*_EXPORT` macro will set it to being exported or
imported, respectively. You should copy an existing file and update the
`*_EXPORT` macro naming for your component.

When defining the target for your component, set:

```python
defines = [ "FOO_IMPLEMENTATION" ]
```

In your BUILD.gn file. If you have source sets that also make up your
component, set this on them also. A good way to share this is to put the
definition in a GN config:

```python
config("foo_implementation") {
  defines = [ "FOO_IMPLEMENTATION" ]
}
```

and set the config on the targets that use it:

```python
configs += [ ":foo_implementation" ]
```

The component build is only reason to use the `*_IMPLEMENTATION` macros. If
your code is not being compiled into a component, don’t define such a macro
(sometimes people do this by copying other targets without understanding).

### Marking symbols for export

Use the `*_EXPORT` macros on function and class declarations (don’t annotate
the implementations) as follows:

```c++
#include "yourcomponent/yourcomponent_export.h"

class YOURCOMPONENT_EXPORT YourClass { ... };

YOURCOMPONENT_EXPORT void SomeFunction();
```

## Creating components from multiple targets

### Static library symbol export issues

Components can be made up of static libraries and GN source sets. A source set
results in all object files from that compilation being linked into the
component. But when code is in a static library, only those object files needed
to define undefined symbols will be pulled in to the link. If an object file is
not needed to link the component itself, it won’t be pulled into the link, even
though it might have exported symbols needed by other components.

Therefore, all code with exported symbols should be either on the component
target itself or in source sets it depends on.

### Splitting targets differently in static and component builds

Sometimes you might have something consisting of multiple sub-targets. For
example: a browser, a renderer, and a common directory, each with their own
target. In the static build, they would all be linked into different places. In
the component build, you may want to have these be in a single component for
performance and sanity reasons. Content is such an example.

The important thing is that the sub-projects not be depended on directly from
outside of the component in the component build. This will duplicate the code
and the import/export of symbols will get confused (see “Common mistakes”
below).

Generally the way to do this is to create browser and renderer group targets
that forward to the right place. In static builds these would forward to
internal targets with the actual code in them. In component builds, these would
forward to the component.

In the static build the structure will be: `//external/thing` ➜ `//foo:browser`
➜ `//foo:browser_impl`

In the component build the structure will be: `//external/thing` ➜
`//foo:browser` ➜ `//foo:mycomponent` ➜ `//foo:browser_impl`

Set GN visibility so that the targets with the code can only be depended on by
targets inside your component.

```python
if (is_component_build) {
  component("mycomponent") {
    public_deps = [ ":browser_impl", ":renderer_impl" ]
  }
}

# External targets always depend on this or the equivalent “renderer” target.
group("browser") {
  if (is_component_build) {
    public_deps = [ ":mycomponent" ]
  } else {
    public_deps = [ ":browser_impl" ]
  }
}

source_set("browser_impl") {
  visibility = [ ":*" ]  # Prevent accidental dependencies.
  defines = [ "IS_MYCOMPONENT_IMPL" ]
  sources = [ ... ]
}
```

## Common mistakes

### Forgetting or misspelling `COMPONENT_EXPORT(*)`

If a function is not marked with your `COMPONENT_EXPORT(FOO)` annotation or the
component name (`FOO`) is misspelled, other components won’t see the symbol when
linking and you’ll get undefined symbols during linking:

    some_file.obj : error LNK2001: unresolved external symbol <some definition>

This will only happen on Windows component builds, which makes the error more
difficult to debug. However, if you see such an error only for Windows
component builds, you know it’s this problem.

### Not defining `IS_*_IMPL` for code in your component

When code is compiled that sees a symbol marked with `__declspec(dllimport)`,
it will expect to find that symbol in another shared library. If that symbol
ends up in the same shared library, you’ll see the error:

    some_file.obj : warning LNK4217: locally defined symbol
    <horrendous mangled name> imported in function <some definition>

The solution is to make sure your `IS_*_IMPL` define is set consistently for all
code in the component. If your component links in source sets or static
libraries, the `IS_*_IMPL` macro must be set on those as well.

### Defining `IS_*_IMPL` for code outside your component

If your `IS_*_IMPL` macro is set for code compiled outside of the
component, that code will expect the symbol to be in the current shared
library, but it won’t be found. It won’t even go looking in other libraries and
the result will be an undefined symbol:

    some_file.obj : error LNK2001: unresolved external symbol <some definition>

### Depending on a source set or static library from both inside and outside a component

If the source set or static library has any `*_EXPORT` macros and ends up both
inside and outside of the component boundary, those symbols will fall under the
cases above where `IS_*_IMPL` is inappropriately defined or inappropriately
undefined. Use GN visibility to make sure callers don’t screw up.

### Putting exported symbols in static libraries

As discussed above, exported symbols should not be in static libraries because
the object file might not be brought into the link. Even if it is brought in
today, it might not be brought in due to completely unrelated changes in the
future. The result will be undefined symbol errors from other components. Use
source sets if your component is made up of more than one target.

### Exporting functions and classes implemented in headers

When you implement a symbol in a header the compiler will put that in every
necessary translation unit and the linker will pick one. If the symbol is never
referenced by code in the shared library it's supposed exported from, it will
never be instantiated and never exported. The result will be undefined external
symbol errors when linking. Exported symbols should be declared in a header but
always implemented in a .cc file.
