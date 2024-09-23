# V8-Blink bindings tips

## How to throw/handle an exception?

There are two options to throw/handle an exception; a) let Blink does it, or b)
let V8 does it.

### Use blink::ExceptionState

The best way is to use `[RaisesException]` IDL extended attribute in *.idl file.
Then, the bindings code generator creates an `ExceptionState` and initializes it
with the API name.  The implementation function takes the `ExceptionState` as
the last argument.  Now you can use the `ExceptionState` to throw and catch an
exception.

```webidl
interface MyInterface {
  // Use [RaisesException] in your *.idl file.
  [RaisesException] void foo(DOMString s);
};
```

```c++
class MyInterface : public ScriptWrappable {
  // The implementation of |foo| takes |ExceptionState&| as the last argument.
  void foo(const StringView& s, ExceptionState& exception_state) {
    // MaybeThrowFunc may throw an exception.
    MaybeThrowFunc(exception_state);
    // Check if MaybeThrowFunc threw an exception or not.
    if (exception_state.HadException())
      return;

    if (s.IsEmpty()) {
      // Throw a ECMAScript TypeError.
      exception_state.ThrowTypeError("s must not be empty");
      return;
    }
  }
};
```

### Use v8::Maybe / v8::MaybeLocal / v8::TryCatch

Sometimes we need to directly use V8 to manage exceptions as not all APIs use
ExceptionState.  Some APIs use `v8::Maybe` / `v8::MaybeLocal` to indicate an
exception.  The important convention is that, if and only if the maybe object is
Nothing, an exception must be thrown.  It's not allowed to return Nothing
without throwing an exception or to throw an exception without returning
Nothing.  Use `TryRethrowScope` to implicitly create a v8::TryCatch and tie it
to an ExceptionState, so that the ExceptionState will pick up the exception.

```c++
void foo(v8::Isolate* isolate, ExceptionState& exception_state) {
  // Puts a v8::TryCatch on the stack, which works like
  // |try { … } catch (e) { … }| in ECMAScript. Will rethrow an exception to
  // ExceptionState in its destructor.
  TryRethrowScope rethrow_scope(isolate, exception_state);

  Type value;
  // ReturnMaybe() returns a v8::Maybe<Type>.
  // v8::Maybe::To is preferred to v8::Maybe::IsJust / IsNothing.
  if (!ReturnMaybe().To(&value)) {
    // An exception occurred, rethrow to ExceptionState on return.
    return;
  }

  v8::Local<V8Type> local_value;
  // ReturnMaybeLocal() returns a v8::MaybeLocal<V8Type>.
  // v8::MaybeLocal::ToLocal is preferred to v8::MaybeLocal::IsJust / IsNothing.
  if (!ReturnMaybeLocal().ToLocal(&local_value)) {
    // An exception occurred, rethrow to ExceptionState on return.
    return;
  }
}
```

## How to retain an ECMAScript value (v8::Value) in a Blink object?

In general, ECMAScript values (v8::Values) are associated with a realm
(v8::Context) and must be protected against a leak across v8::Context and/or
isolated worlds.  You must be extra careful when storing a v8::Value in a Blink
object.

The best way is to avoid storing a v8::Value in Blink, however, if you end up
storing a v8::Value in a Blink object, there are two options;
`TraceWrapperV8Reference` and `WorldSafeWrapperReference`.

`TraceWrapperV8Reference<V8Type>` works just like Member<BlinkType> and makes the
V8Type value alive as long as it's traced via `Trace` member function.  If
you're pretty sure that the value never be accessible across isolated worlds,
this is the default choice.

`WorldSafeWrapperReference<V8Type>` is recommended if the value is accessible
across isolated worlds and/or if there are any security concerns.  Compared to
TraceWrapperV8Reference, WorldSafeWrapperReference provides extra checks against
cross-world access and object-cloning across isolated worlds.
WorldSafeWrapperReference doesn't prevent information leak, but it prevents world
leak.  An example use case is Event.  Events are often dispatched not only in
the main world but also in isolated worlds, too.  Event objects may be
accessible in a variety of worlds, so it's a good choice to use
WorldSafeWrapperReference to store a v8::Value in an Event object.

`ScriptValue`, `v8::Persistent`, `v8::Global`, and `v8::Eternal` are _NOT_
recommended as a way of storing v8::Value in a Blink object.  They had been used
in the past, but they're now obsolete after the unified heap project.  Now
TraceWrapperV8Reference and WorldSafeWrapperReference are recommended.
