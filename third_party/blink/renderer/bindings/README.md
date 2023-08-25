# The bindings/ directory

bindings/ contains code for V8-DOM bindings.
bindings/ is a layer that implements high-performance C++ bindings
between V8 and DOM per the [Web IDL spec](https://webidl.spec.whatwg.org/).

## Directory structure

bindings/ consists of the following directories:

* `Source/bindings/scripts/` implements the
[IDLCompiler.md](IDLCompiler.md)
in Python. The IDL compiler is responsible for auto-generating C++ bindings code
for all Web IDL files per the Web IDL spec.

* `Source/bindings/templates/` is a set of Jinja templates used
by the IDL compiler.

* `//out/*/gen/blink/bindings/` is a directory where the auto-generated
code is generated.

* `Source/bindings/tests/` is a set of tests for the IDL compiler.
You can run the tests by `tools/run_bindings_tests.py`.

* `Source/platform/bindings/`, `Source/bindings/core/` and
`Source/bindings/modules/` provide a bunch of utility classes that encapsulate
the complexity of bare V8 APIs. Utility classes that purely depend on V8 APIs
go to `Source/platform/bindings/`. Utility classes that depend on `Source/core/`
go to `Source/bindings/core/`. Utility classes that depend on `Source/modules/`
go to `Source/bindings/modules/`. Since V8 APIs are low-level and error-prone,
`Souce/core/` and `Source/modules/` are encouraged to use the utility classes
instead of directly using the bare V8 APIs. If a class in
`Source/core/` or `Source/modules/` is using a bunch of bare V8 APIs,
consider moving the class to `Source/bindings/core/` and `Source/bindings/modules/`.
The point is that we should put all the complexity around V8 APIs into
`Source/bindings/` so that the code ie kept under control of the binding team.

## Resources

* [IDLCompiler.md](IDLCompiler.md)
explains the IDL compiler.

* [V8BindingDesign.md](core/v8/V8BindingDesign.md)
explains basic concepts of the bindings architecture; e.g.,
Isolate, Context, World, wrapper objects etc.

* [TraceWrapperReference.md](core/v8/TraceWrapperReference.md)
explains how the lifetime of wrapper objects is maintained by the V8 GC.

* [IDLExtendedAttributes.md](IDLExtendedAttributes.md)
explains all extended IDL attributes used in Web IDL files in Blink.
If you have any question about IDL files, see this document.

* [IDLUnionTypes.md](IDLUnionTypes.md)
explains how to use IDL union types.

* [v8.h](https://cs.chromium.org/chromium/src/v8/include/v8.h?sq=package:chromium&dr=CSs)
defines V8 APIs with good documentation.

## When you use V8 APIs, ask blink-reviews-bindings@ for review!

V8 APIs are sometimes hard to use. It's easy to use a wrong v8::Context,
it's easy to leak window objects, it's easy to ignore exceptions incorrectly
etc. It is not disallowed to use bare V8 APIs in
`Source/core/` and `Source/modules/` but when you use V8 APIs, please ask
blink-reviews-bindings@chromium.org for review.

## Contact

If you have any question, ask blink-reviews-bindings@chromium.org.
