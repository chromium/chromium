# Wrapper Tracing Reference

*Wrapper tracing is deprecated* and not in use anymore. Blink uses *unified heap* these days which just relies on regular Oilpan types for C++.

In case you are looking for how to keep JavaScript wrapper alive from C++ using tracing use the following:
- Use `Member<T>` for any managed pointer, independent of whether a JavaScript object is transitively reachable or not. With wrapper tracing interesting references required manually annotating  with `TraceWrapperMember<T>`. This is not needed anymore; in fact, the type does not exist anymore.
- Use `TraceWrapperV8Reference<T>` to annotate references to V8 that this object should keep alive.

