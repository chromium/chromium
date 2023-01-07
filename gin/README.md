Gin - Lightweight bindings for V8
=================================

This directory contains Gin, a set of utilities to make working with V8 easier.

Here are some of the key bits:

* converter.h: Templatized JS &harr; C++ conversion routines for many common C++
  types. You can define your own by specializing Converter.

* function\_template.h: Create JavaScript functions that dispatch to any C++
  function, member function pointer, or base::RepeatingCallback.

* object\_template\_builder.h: A handy utility for creation of v8::ObjectTemplate.

* wrappable.h: Base class for C++ classes that want to be owned by the V8 GC.
  Wrappable objects are automatically deleted when GC discovers that nothing in
  the V8 heap refers to them. This is also an easy way to expose C++ objects to
  JavaScript.

* runner.h: Create script contexts and run code in them.

* module\_runner\_delegate.h: A delegate for runner that implements a subset of
  the AMD module specification. Also see modules/ with some example modules.
