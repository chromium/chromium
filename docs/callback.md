# Callback<> and Bind()

[TOC]

## Introduction

The templated `base::Callback<>` class is a generalized function object.
Together with the `base::Bind()` function in base/bind.h, they provide a
type-safe method for performing partial application of functions.

Partial application (or "currying") is the process of binding a subset of a
function's arguments to produce another function that takes fewer arguments.
This can be used to pass around a unit of delayed execution, much like lexical
closures are used in other languages. For example, it is used in Chromium code
to schedule tasks on different MessageLoops.

A callback with no unbound input parameters (`base::Callback<void()>`) is
called a `base::Closure`. Note that this is NOT the same as what other
languages refer to as a closure -- it does not retain a reference to its
enclosing environment.

### OnceCallback<> And RepeatingCallback<>

`base::OnceCallback<>` and `base::RepeatingCallback<>` are next gen callback
classes, which are under development.

`base::OnceCallback<>` is created by `base::BindOnce()`. This is a callback
variant that is a move-only type and can be run only once. This moves out bound
parameters from its internal storage to the bound function by default, so it's
easier to use with movable types. This should be the preferred callback type:
since the lifetime of the callback is clear, it's simpler to reason about when
a callback that is passed between threads is destroyed.

`base::RepeatingCallback<>` is created by `base::BindRepeating()`. This is a
callback variant that is copyable that can be run multiple times. It uses
internal ref-counting to make copies cheap. However, since ownership is shared,
it is harder to reason about when the callback and the bound state are
destroyed, especially when the callback is passed between threads.

The legacy `base::Callback<>` is currently aliased to
`base::RepeatingCallback<>`. In new code, prefer `base::OnceCallback<>` where
possible, and use `base::RepeatingCallback<>` otherwise. Once the migration is
complete, the type alias will be removed and `base::OnceCallback<>` will be renamed
to `base::Callback<>` to emphasize that it should be preferred.

`base::RepeatingCallback<>` is convertible to `base::OnceCallback<>` by the
implicit conversion.

### Memory Management And Passing

Pass `base::{Once,Repeating}Callback` objects by value if ownership is
transferred; otherwise, pass it by const-reference.

```cpp
// |Foo| just refers to |cb| but doesn't store it nor consume it.
bool Foo(const base::OnceCallback<void(int)>& cb) {
  return cb.is_null();
}

// |Bar| takes the ownership of |cb| and stores |cb| into |g_cb|.
base::RepeatingCallback<void(int)> g_cb;
void Bar(base::RepeatingCallback<void(int)> cb) {
  g_cb = std::move(cb);
}

// |Baz| takes the ownership of |cb| and consumes |cb| by Run().
void Baz(base::OnceCallback<void(int)> cb) {
  std::move(cb).Run(42);
}

// |Qux| takes the ownership of |cb| and transfers ownership to PostTask(),
// which also takes the ownership of |cb|.
void Qux(base::RepeatingCallback<void(int)> cb) {
  PostTask(FROM_HERE, base::BindOnce(cb, 42));
  PostTask(FROM_HERE, base::BindOnce(std::move(cb), 43));
}
```

When you pass a `base::{Once,Repeating}Callback` object to a function parameter,
use `std::move()` if you don't need to keep a reference to it, otherwise, pass the
object directly. You may see a compile error when the function requires the
exclusive ownership, and you didn't pass the callback by move. Note that the
moved-from `base::{Once,Repeating}Callback` becomes null, as if its `Reset()`
method had been called. Afterward, its `is_null()` method will return true and
its `operator bool()` will return false.

## Quick reference for basic stuff

### Binding A Bare Function

```cpp
int Return5() { return 5; }
base::OnceCallback<int()> func_cb = base::BindOnce(&Return5);
LOG(INFO) << std::move(func_cb).Run();  // Prints 5.
```

```cpp
int Return5() { return 5; }
base::RepeatingCallback<int()> func_cb = base::BindRepeating(&Return5);
LOG(INFO) << func_cb.Run();  // Prints 5.
```

### Binding A Captureless Lambda

```cpp
base::Callback<int()> lambda_cb = base::Bind([] { return 4; });
LOG(INFO) << lambda_cb.Run();  // Print 4.

base::OnceCallback<int()> lambda_cb2 = base::BindOnce([] { return 3; });
LOG(INFO) << std::move(lambda_cb2).Run();  // Print 3.
```

### Binding A Capturing Lambda (In Tests)

When writing tests, it is often useful to capture arguments that need to be
modified in a callback.

``` cpp
#include "base/test/bind_test_util.h"

int i = 2;
base::Callback<void()> lambda_cb = base::BindLambdaForTesting([&]() { i++; });
lambda_cb.Run();
LOG(INFO) << i;  // Print 3;
```

### Binding A Class Method

The first argument to bind is the member function to call, the second is the
object on which to call it.

```cpp
class Ref : public base::RefCountedThreadSafe<Ref> {
 public:
  int Foo() { return 3; }
};
scoped_refptr<Ref> ref = new Ref();
base::Callback<void()> ref_cb = base::Bind(&Ref::Foo, ref);
LOG(INFO) << ref_cb.Run();  // Prints out 3.
```

By default the object must support RefCounted or you will get a compiler
error. If you're passing between threads, be sure it's RefCountedThreadSafe! See
"Advanced binding of member functions" below if you don't want to use reference
counting.

### Running A Callback

Callbacks can be run with their `Run` method, which has the same signature as
the template argument to the callback. Note that `base::OnceCallback::Run`
consumes the callback object and can only be invoked on a callback rvalue.

```cpp
void DoSomething(const base::Callback<void(int, std::string)>& callback) {
  callback.Run(5, "hello");
}

void DoSomethingOther(base::OnceCallback<void(int, std::string)> callback) {
  std::move(callback).Run(5, "hello");
}
```

RepeatingCallbacks can be run more than once (they don't get deleted or marked
when run). However, this precludes using `base::Passed` (see below).

```cpp
void DoSomething(const base::RepeatingCallback<double(double)>& callback) {
  double myresult = callback.Run(3.14159);
  myresult += callback.Run(2.71828);
}
```

If running a callback could result in its own destruction (e.g., if the callback
recipient deletes the object the callback is a member of), the callback should
be moved before it can be safely invoked. (Note that this is only an issue for
RepeatingCallbacks, because a OnceCallback always has to be moved for
execution.)

```cpp
void Foo::RunCallback() {
  std::move(&foo_deleter_callback_).Run();
}
```

### Creating a Callback That Does Nothing

Sometimes you need a callback that does nothing when run (e.g. test code that
doesn't care to be notified about certain types of events).  It may be tempting
to pass a default-constructed callback of the right type:

```cpp
using MyCallback = base::OnceCallback<void(bool arg)>;
void MyFunction(MyCallback callback) {
  std::move(callback).Run(true);  // Uh oh...
}
...
MyFunction(MyCallback());  // ...this will crash when Run()!
```

Default-constructed callbacks are null, and thus cannot be Run().  Instead, use
`base::DoNothing()`:

```cpp
...
MyFunction(base::DoNothing());  // Can be Run(), will no-op
```

`base::DoNothing()` can be passed for any OnceCallback or RepeatingCallback that
returns void.

Implementation-wise, `base::DoNothing()` is actually a functor which produces a
callback from `operator()`.  This makes it unusable when trying to bind other
arguments to it.  Normally, the only reason to bind arguments to DoNothing() is
to manage object lifetimes, and in these cases, you should strive to use idioms
like DeleteSoon(), ReleaseSoon(), or RefCountedDeleteOnSequence instead.  If you
truly need to bind an argument to DoNothing(), or if you need to explicitly
create a callback object (because implicit conversion through operator()() won't
compile), you can instantiate directly:

```cpp
// Binds |foo_ptr| to a no-op OnceCallback takes a scoped_refptr<Foo>.
// ANTIPATTERN WARNING: This should likely be changed to ReleaseSoon()!
base::Bind(base::DoNothing::Once<scoped_refptr<Foo>>(), foo_ptr);
```

### Passing Unbound Input Parameters

Unbound parameters are specified at the time a callback is `Run()`. They are
specified in the `base::Callback` template type:

```cpp
void MyFunc(int i, const std::string& str) {}
base::Callback<void(int, const std::string&)> cb = base::Bind(&MyFunc);
cb.Run(23, "hello, world");
```

### Passing Bound Input Parameters

Bound parameters are specified when you create the callback as arguments to
`base::Bind()`. They will be passed to the function and the `Run()`ner of the
callback doesn't see those values or even know that the function it's calling.

```cpp
void MyFunc(int i, const std::string& str) {}
base::Callback<void()> cb = base::Bind(&MyFunc, 23, "hello world");
cb.Run();
```

A callback with no unbound input parameters (`base::Callback<void()>`) is
called a `base::Closure`. So we could have also written:

```cpp
base::Closure cb = base::Bind(&MyFunc, 23, "hello world");
```

When calling member functions, bound parameters just go after the object
pointer.

```cpp
base::Closure cb = base::Bind(&MyClass::MyFunc, this, 23, "hello world");
```

### Partial Binding Of Parameters (Currying)

You can specify some parameters when you create the callback, and specify the
rest when you execute the callback.

When calling a function bound parameters are first, followed by unbound
parameters.

```cpp
void ReadIntFromFile(const std::string& filename,
                     base::OnceCallback<void(int)> on_read);

void DisplayIntWithPrefix(const std::string& prefix, int result) {
  LOG(INFO) << prefix << result;
}

void AnotherFunc(const std::string& file) {
  ReadIntFromFile(file, base::BindOnce(&DisplayIntWithPrefix, "MyPrefix: "));
};
```

This technique is known as [Currying](http://en.wikipedia.org/wiki/Currying). It
should be used in lieu of creating an adapter class that holds the bound
arguments. Notice also that the `"MyPrefix: "` argument is actually a
`const char*`, while `DisplayIntWithPrefix` actually wants a
`const std::string&`. Like normal function dispatch, `base::Bind`, will coerce
parameter types if possible.

### Avoiding Copies With Callback Parameters

A parameter of `base::BindRepeating()` or `base::BindOnce()` is moved into its
internal storage if it is passed as a rvalue.

```cpp
std::vector<int> v = {1, 2, 3};
// |v| is moved into the internal storage without copy.
base::Bind(&Foo, std::move(v));
```

```cpp
// The vector is moved into the internal storage without copy.
base::Bind(&Foo, std::vector<int>({1, 2, 3}));
```

Arguments bound with `base::BindOnce()` are always moved, if possible, to the
target function.
A function parameter that is passed by value and has a move constructor will be
moved instead of copied.
This makes it easy to use move-only types with `base::BindOnce()`.

In contrast, arguments bound with `base::BindRepeating()` are only moved to the
target function if the argument is bound with `base::Passed()`.

**DANGER**:
A `base::RepeatingCallback` can only be run once if arguments were bound with
`base::Passed()`.
For this reason, avoid `base::Passed()`.
If you know a callback will only be called once, prefer to refactor code to
work with `base::OnceCallback` instead.

Avoid using `base::Passed()` with `base::BindOnce()`, as `std::move()` does the
same thing and is more familiar.

```cpp
void Foo(std::unique_ptr<int>) {}
auto p = std::make_unique<int>(42);

// |p| is moved into the internal storage of Bind(), and moved out to |Foo|.
base::BindOnce(&Foo, std::move(p));
base::BindRepeating(&Foo, base::Passed(&p)); // Ok, but subtle.
base::BindRepeating(&Foo, base::Passed(std::move(p))); // Ok, but subtle.
```

## Quick reference for advanced binding

### Binding A Class Method With Weak Pointers

If `MyClass` has a `base::WeakPtr<MyClass> weak_this_` member (see below)
then a class method can be bound with:

```cpp
base::Bind(&MyClass::Foo, weak_this_);
```

The callback will not be run if the object has already been destroyed.

Note that class method callbacks bound to `base::WeakPtr`s may only be
run on the same sequence on which the object will be destroyed, since otherwise
execution of the callback might race with the object's deletion.

To use `base::WeakPtr` with `base::Bind()`, `MyClass` will typically look like:

```cpp
class MyClass {
public:
  MyClass() {
    weak_this_ = weak_factory_.GetWeakPtr();
  }
private:
  base::WeakPtr<MyClass> weak_this_;
  // MyClass member variables go here.
  base::WeakPtrFactory<MyClass> weak_factory_{this};
};
```

`weak_factory_` is the last member variable in `MyClass` so that it is
destroyed first. This ensures that if any class methods bound to `weak_this_`
are `Run()` during teardown, then they will not actually be executed.

If `MyClass` only ever `base::Bind()`s and executes callbacks on the same
sequence, then it is generally safe to call `weak_factory_.GetWeakPtr()` at the
`base::Bind()` call, rather than taking a separate `weak_this_` during
construction.

### Binding A Class Method With Manual Lifetime Management

```cpp
base::Bind(&MyClass::Foo, base::Unretained(this));
```

This disables all lifetime management on the object. You're responsible for
making sure the object is alive at the time of the call. You break it, you own
it!

### Binding A Class Method And Having The Callback Own The Class

```cpp
MyClass* myclass = new MyClass;
base::Bind(&MyClass::Foo, base::Owned(myclass));
```

The object will be deleted when the callback is destroyed, even if it's not run
(like if you post a task during shutdown). Potentially useful for "fire and
forget" cases.

Smart pointers (e.g. `std::unique_ptr<>`) are also supported as the receiver.

```cpp
std::unique_ptr<MyClass> myclass(new MyClass);
base::Bind(&MyClass::Foo, std::move(myclass));
```

### Ignoring Return Values

Sometimes you want to call a function that returns a value in a callback that
doesn't expect a return value.

```cpp
int DoSomething(int arg) { cout << arg << endl; }
base::Callback<void(int)> cb =
    base::Bind(IgnoreResult(&DoSomething));
```

## Quick reference for binding parameters to Bind()

Bound parameters are specified as arguments to `base::Bind()` and are passed to
the function. A callback with no parameters or no unbound parameters is called
a `base::Closure` (`base::Callback<void()>` and `base::Closure` are the same
thing).

### Passing Parameters Owned By The Callback

```cpp
void Foo(int* arg) { cout << *arg << endl; }
int* pn = new int(1);
base::Closure foo_callback = base::Bind(&foo, base::Owned(pn));
```

The parameter will be deleted when the callback is destroyed, even if it's not
run (like if you post a task during shutdown).

### Passing Parameters As A unique_ptr

```cpp
void TakesOwnership(std::unique_ptr<Foo> arg) {}
auto f = std::make_unique<Foo>();
// f becomes null during the following call.
base::OnceClosure cb = base::BindOnce(&TakesOwnership, std::move(f));
```

Ownership of the parameter will be with the callback until the callback is run,
and then ownership is passed to the callback function. This means the callback
can only be run once. If the callback is never run, it will delete the object
when it's destroyed.

### Passing Parameters As A scoped_refptr

```cpp
void TakesOneRef(scoped_refptr<Foo> arg) {}
scoped_refptr<Foo> f(new Foo);
base::Closure cb = base::Bind(&TakesOneRef, f);
```

This should "just work." The closure will take a reference as long as it is
alive, and another reference will be taken for the called function.

```cpp
void DontTakeRef(Foo* arg) {}
scoped_refptr<Foo> f(new Foo);
base::Closure cb = base::Bind(&DontTakeRef, base::RetainedRef(f));
```

`base::RetainedRef` holds a reference to the object and passes a raw pointer to
the object when the Callback is run.

### Passing Parameters By Reference

References are *copied* unless `std::ref` or `std::cref` is used. Example:

```cpp
void foo(const int& arg) { printf("%d %p\n", arg, &arg); }
int n = 1;
base::Closure has_copy = base::Bind(&foo, n);
base::Closure has_ref = base::Bind(&foo, std::cref(n));
n = 2;
foo(n);                        // Prints "2 0xaaaaaaaaaaaa"
has_copy.Run();                // Prints "1 0xbbbbbbbbbbbb"
has_ref.Run();                 // Prints "2 0xaaaaaaaaaaaa"
```

Normally parameters are copied in the closure.
**DANGER**: `std::ref` and `std::cref` store a (const) reference instead,
referencing the original parameter. This means that you must ensure the object
outlives the callback!

## Implementation notes

### Where Is This Design From:

The design of `base::Callback` and `base::Bind` is heavily influenced by C++'s
`tr1::function` / `tr1::bind`, and by the "Google Callback" system used inside
Google.

### Customizing the behavior

There are several injection points that controls binding behavior from outside
of its implementation.

```cpp
namespace base {

template <typename Receiver>
struct IsWeakReceiver {
  static constexpr bool value = false;
};

template <typename Obj>
struct UnwrapTraits {
  template <typename T>
  T&& Unwrap(T&& obj) {
    return std::forward<T>(obj);
  }
};

}  // namespace base
```

If `base::IsWeakReceiver<Receiver>::value` is true on a receiver of a method,
`base::Bind` checks if the receiver is evaluated to true and cancels the invocation
if it's evaluated to false. You can specialize `base::IsWeakReceiver` to make
an external smart pointer as a weak pointer.

`base::UnwrapTraits<BoundObject>::Unwrap()` is called for each bound arguments
right before `base::Callback` calls the target function. You can specialize this
to define an argument wrapper such as `base::Unretained`, `base::Owned`,
`base::RetainedRef` and `base::Passed`.

### How The Implementation Works:

There are three main components to the system:
  1) The `base::Callback<>` classes.
  2) The `base::Bind()` functions.
  3) The arguments wrappers (e.g., `base::Unretained()` and `base::Owned()`).

The Callback classes represent a generic function pointer. Internally, it
stores a refcounted piece of state that represents the target function and all
its bound parameters. The `base::Callback` constructor takes a
`base::BindStateBase*`, which is upcasted from a `base::BindState<>`. In the
context of the constructor, the static type of this `base::BindState<>` pointer
uniquely identifies the function it is representing, all its bound parameters,
and a `Run()` method that is capable of invoking the target.

`base::Bind()` creates the `base::BindState<>` that has the full static type,
and erases the target function type as well as the types of the bound
parameters. It does this by storing a pointer to the specific `Run()` function,
and upcasting the state of `base::BindState<>*` to a `base::BindStateBase*`.
This is safe as long as this `BindStateBase` pointer is only used with the
stored `Run()` pointer.

To `base::BindState<>` objects are created inside the `base::Bind()` functions.
These functions, along with a set of internal templates, are responsible for

 - Unwrapping the function signature into return type, and parameters
 - Determining the number of parameters that are bound
 - Creating the BindState storing the bound parameters
 - Performing compile-time asserts to avoid error-prone behavior
 - Returning a `Callback<>` with an arity matching the number of unbound
   parameters and that knows the correct refcounting semantics for the
   target object if we are binding a method.

The `base::Bind` functions do the above using type-inference and variadic
templates.

By default `base::Bind()` will store copies of all bound parameters, and
attempt to refcount a target object if the function being bound is a class
method. These copies are created even if the function takes parameters as const
references. (Binding to non-const references is forbidden, see bind.h.)

To change this behavior, we introduce a set of argument wrappers (e.g.,
`base::Unretained()`).  These are simple container templates that are passed by
value, and wrap a pointer to argument.  See the file-level comment in
base/bind_helpers.h for more info.

These types are passed to the `Unwrap()` functions to modify the behavior of
`base::Bind()`.  The `Unwrap()` functions change behavior by doing partial
specialization based on whether or not a parameter is a wrapper type.

`base::Unretained()` is specific to Chromium.

### Missing Functionality
 - Binding arrays to functions that take a non-const pointer.
   Example:
```cpp
void Foo(const char* ptr);
void Bar(char* ptr);
base::Bind(&Foo, "test");
base::Bind(&Bar, "test");  // This fails because ptr is not const.
```
 - In case of partial binding of parameters a possibility of having unbound
   parameters before bound parameters. Example:
```cpp
void Foo(int x, bool y);
base::Bind(&Foo, _1, false); // _1 is a placeholder.
```

If you are thinking of forward declaring `base::Callback` in your own header
file, please include "base/callback_forward.h" instead.
