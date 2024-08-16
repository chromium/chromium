# OnceCallback<> and BindOnce(), RepeatingCallback<> and BindRepeating()

[TOC]

## Introduction

The templated `base::{Once, Repeating}Callback<>` classes are generalized
function objects. Together with the `base::Bind{Once, Repeating}()` functions in
base/functional/bind.h, they provide a type-safe method for performing partial
application of functions.

Partial application is the process of binding a subset of a function's arguments
to produce another function that takes fewer arguments. This can be used to pass
around a unit of delayed execution, much like lexical closures are used in other
languages. For example, it is used in Chromium code to schedule tasks on
different MessageLoops.

A callback with no unbound input parameters (`base::OnceCallback<void()>`) is
called a `base::OnceClosure`. The same pattern exists for
base::RepeatingCallback, as `base::RepeatingClosure`. Note that this is NOT the
same as what other languages refer to as a closure -- it does not retain a
reference to its enclosing environment.

### OnceCallback<> And RepeatingCallback<>

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

Prefer `base::OnceCallback<>` where possible, and use `base::RepeatingCallback<>`
otherwise.

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

### Chaining callbacks

When you have 2 callbacks that you wish to run in sequence, they can be joined
together into a single callback through the use of `Then()`.

Calling `Then()` on a `base::OnceCallback` joins a second callback that will be
run together with, but after, the first callback. The return value from the
first callback is passed along to the second, and the return value from the
second callback is returned at the end. More concretely, calling `a.Then(b)`
produces a new `base::OnceCallback` that will run `b(a());`, returning the
result from `b`.

This example uses `Then()` to join 2 `base::OnceCallback`s together:
```cpp
int Floor(float f) { return std::floor(f); }
std::string IntToString(int i) { return base::NumberToString(i); }

base::OnceCallback<int(float)> first = base::BindOnce(&Floor);
base::OnceCallback<std::string(int)> second = base::BindOnce(&IntToString);

// This will run |first|, run and pass the result to |second|, then return
// the result from |second|.
std::string r = std::move(first).Then(std::move(second)).Run(3.5f);
// |r| will be "3". |first| and |second| are now both null, as they were
// consumed to perform the join operation.
```

Similarly, `Then()` also works with `base::RepeatingCallback`; however, the
joined callback must also be a `base::RepeatingCallback` to ensure the resulting
callback can be invoked multiple times.

This example uses `Then()` to join 2 `base::RepeatingCallback`s together:
```cpp
int Floor(float f) { return std::floor(f); }
std::string IntToString(int i) { return base::NumberToString(i); }

base::RepeatingCallback<int(float)> first = base::BindRepeating(&Floor);
base::RepeatingCallback<std::string(int)> second = base::BindRepeating(&IntToString);

// This creates a RepeatingCallback that will run |first|, run and pass the
// result to |second|, then return the result from |second|.
base::RepeatingCallback<std::string(float)> joined =
    std::move(first).Then(std::move(second));
// |first| and |second| are now both null, as they were consumed to perform
// the join operation.

// This runs the functor that was originally bound to |first|, then |second|.
std::string r = joined.Run(3.5);
// |r| will be "3".

// It's valid to call it multiple times since all callbacks involved are
// base::RepeatingCallbacks.
r = joined.Run(2.5);
// |r| is set to "2".
```

In the above example, casting the `base::RepeatingCallback` to an r-value with
`std::move()` causes `Then()` to destroy the original callback, in the same way
that occurs for joining `base::OnceCallback`s. However since a
`base::RepeatingCallback` can be run multiple times, it can be joined
non-destructively as well.
```cpp
int Floor(float f) { return std::floor(f); }
std::string IntToString(int i) { return base::NumberToString(i); }

base::RepeatingCallback<int(float)> first = base::BindRepeating(&Floor);
base::RepeatingCallback<std::string(int)> second = base::BindRepeating(&IntToString);

// This creates a RepeatingCallback that will run |first|, run and pass the
// result to |second|, then return the result from |second|.
std::string r = first.Then(second).Run(3.5f);
// |r| will be 3, and |first| and |second| are still valid to use.

// Runs Floor().
int i = first.Run(5.5);
// Runs IntToString().
std::string s = second.Run(9);
```

If the second callback does not want to receive a value from the first callback,
you may use `base::IgnoreResult` to drop the return value in between running the
two.

```cpp
// Returns an integer.
base::RepeatingCallback<int()> first = base::BindRepeating([](){ return 5; });
// Does not want to receive an integer.
base::RepeatingClosure second = base::BindRepeating([](){});

// This will not compile, because |second| can not receive the return value from
// |first|.
//   first.Then(second).Run();

// We can drop the result from |first| before running second.
base::BindRepeating(base::IgnoreResult(first)).Then(second).Run();
// This will effectively create a callback that when Run() will call
// `first(); second();` instead of `second(first());`.
```

Note that the return value from |first| will be lost in the above example, and
would be destroyed before |second| is run. If you want the return value from
|first| to be preserved and ultimately returned after running both |first| and
|second|, then you would need a primitive such as the  `base::PassThrough<T>()`
helper in the [base::PassThrough CL](https://chromium-review.googlesource.com/c/chromium/src/+/2493243).
If this would be helpful for you, please let danakj@chromium.org know or ping
the CL.

### Chaining callbacks across different task runners

```cpp
// The task runner for a different thread.
scoped_refptr<base::SequencedTaskRunner> other_task_runner = ...;

// A function to compute some interesting result, except it can only be run
// safely from `other_task_runner` and not the current thread.
int ComputeResult();

base::OnceCallback<int()> compute_result_cb = base::BindOnce(&ComputeResult);

// Task runner for the current thread.
scoped_refptr<base::SequencedTaskRunner> current_task_runner =
    base::SequencedTaskRunner::GetCurrentDefault();

// A function to accept the result, except it can only be run safely from the
// current thread.
void ProvideResult(int result);

base::OnceCallback<void(int)> provide_result_cb =
    base::BindOnce(&ProvideResult);
```

Using `Then()` to join `compute_result_cb` and `provide_result_cb` directly
would be inappropriate. `ComputeResult()` and `ProvideResult()` would run on the
same thread which isn't safe. However, `base::BindPostTask()` can be used to
ensure `provide_result_cb` will run on `current_task_runner`.

```cpp
// The following two statements post a task to `other_task_runner` to run
// `task`. This will invoke ComputeResult() on a different thread to get the
// result value then post a task back to `current_task_runner` to invoke
// ProvideResult() with the result.
OnceClosure task =
    std::move(compute_result_cb)
        .Then(base::BindPostTask(current_task_runner,
                                 std::move(provide_result_cb)));
other_task_runner->PostTask(FROM_HERE, std::move(task));
```

### Splitting a OnceCallback in two

If a callback is only run once, but two references need to be held to the
callback, using a `base::OnceCallback` can be clearer than a
`base::RepeatingCallback`, from an intent and semantics point of view.
`base::SplitOnceCallback()` takes a `base::OnceCallback` and returns a pair of
callbacks with the same signature. When either of the returned callback is run,
the original callback is invoked. Running the leftover callback will result in a
crash.
This can be useful when passing a `base::OnceCallback` to a function that may or
may not take ownership of the callback. E.g, when an object creation could fail:

```cpp
std::unique_ptr<FooTask> CreateFooTask(base::OnceClosure task) {
  std::pair<base::OnceClosure,base::OnceClosure> split
                                    = base::SplitOnceCallback(std::move(task));

  std::unique_ptr<FooTask> foo = TryCreateFooTask(std::move(split.first));
  if (foo)
    return foo;

  return CreateFallbackFooTask(std::move(split.second));
}
```

While it is best to use a single callback to report success/failure, some APIs
already take multiple callbacks. `base::SplitOnceCallback()` can be used to
split a completion callback and help in such a case:

```cpp
using StatusCallback = base::OnceCallback<void(FooStatus)>;
void DoOperation(StatusCallback done_cb) {
  std::pair<StatusCallback, StatusCallback> split
                                 = base::SplitOnceCallback(std::move(done_cb));

  InnerWork(BindOnce(std::move(split.first), STATUS_OK),
            BindOnce(std::move(split.second), STATUS_ABORTED));
}

void InnerWork(base::OnceClosure work_done_cb,
               base::OnceClosure work_aborted_cb);
```

### BarrierCallback<T>

Sometimes you might need to request data from several sources, then do something
with the collective results once all data is available. You can do this with a
`BarrierCallback<T>`. The `BarrierCallback<T>` is created with two parameters:

-   `num_callbacks`: The number of times the `BarrierCallback` can be run, each
    time being passed an object of type T.
-   `done_callback`: This will be run once the `BarrierCallback` has been run
    `num_callbacks` times.

The `done_callback` will receive a `std::vector<T>` containing the
`num_callbacks` parameters passed in the respective `Run` calls. The order of
`Ts` in the `vector` is unspecified.

Note that

-   barrier callback must not be run more than `num_callback` times,
-   `done_callback` will be called on the same thread as the final call to the
    barrier callback. `done_callback` will also be cleared on the same thread.

Example:

```cpp
void Merge(const std::vector<Data>& data);

void Collect(base::OnceCallback<void(Data)> collect_and_merge) {
  // Do something, probably asynchronously, and at some point:
  std::move(collect_and_merge).Run(data);
}

CollectAndMerge() {
  const auto collect_and_merge =
      base::BarrierCallback<Data>(sources_.size(), base::BindOnce(&Merge));
  for (const auto& source : sources_) {
    // Copy the barrier callback for asynchronous data collection.
    // Once all sources have called `collect_and_merge` with their respective
    // data, |Merge| will be called with a vector of the collected data.
    source.Collect(collect_and_merge);
  }
}
```

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
base::RepeatingCallback<int()> lambda_cb = base::BindRepeating([] { return 4; });
LOG(INFO) << lambda_cb.Run();  // Print 4.

base::OnceCallback<int()> lambda_cb2 = base::BindOnce([] { return 3; });
LOG(INFO) << std::move(lambda_cb2).Run();  // Print 3.

base::OnceCallback<int()> lambda_cb3 = base::BindOnce([] { return 2; });
base::OnceCallback<int(base::OnceCallback<int()>)> lambda_cb4 =
    base::BindOnce(
        [](base::OnceCallback<int()> callback) {
            return std::move(callback).Run(); },
        std::move(lambda_cb3));
LOG(INFO) << std::move(lambda_cb4).Run();  // Print 2.

```

### Binding A Capturing Lambda (In Tests)

When writing tests, it is often useful to capture arguments that need to be
modified in a callback.

``` cpp
#include "base/test/bind.h"

int i = 2;
base::RepeatingCallback<void()> lambda_cb = base::BindLambdaForTesting([&]() { i++; });
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
base::RepeatingCallback<void()> ref_cb = base::BindRepeating(&Ref::Foo, ref);
LOG(INFO) << ref_cb.Run();  // Prints out 3.
```

By default the object must support RefCounted or you will get a compiler
error. If you're passing between threads, be sure it's RefCountedThreadSafe! See
"Advanced binding of member functions" below if you don't want to use reference
counting.

Binding a non-const method with a const object is not allowed, for example:

```cpp
class MyClass {
 public:
  base::OnceClosure GetCallback() const {
    base::BindOnce(
        // A template error will prevent the non-const method from being bound
        // to the the WeakPtr<const MyClass>.
        &MyClass::OnCallback,
        weak_factory_.GetWeakPtr());
  }

 private:
  void OnCallback(); // non-const
  base::WeakPtrFactory<MyClass> weak_factory_{this};
}
```

### Running A Callback

Callbacks can be run with their `Run` method, which has the same signature as
the template argument to the callback. Note that `base::OnceCallback::Run`
consumes the callback object and can only be invoked on a callback rvalue.

```cpp
void DoSomething(const base::RepeatingCallback<void(int, std::string)>& callback) {
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
be moved or copied onto the stack before it can be safely invoked. (Note that
this is only an issue for RepeatingCallbacks, because a OnceCallback always has
to be moved for execution.)

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
base::BindOnce(base::DoNothingAs<void(scoped_refptr<Foo>)>(), foo_ptr);
```

### Passing Unbound Input Parameters

Unbound parameters are specified at the time a callback is `Run()`. They are
specified in the `base::{Once, Repeating}Callback` template type:

```cpp
void MyFunc(int i, const std::string& str) {}
base::RepeatingCallback<void(int, const std::string&)> cb = base::BindRepeating(&MyFunc);
cb.Run(23, "hello, world");
```

### Passing Bound Input Parameters

Bound parameters are specified when you create the callback as arguments to
`base::Bind{Once, Repeating}()`. They will be passed to the function and the `Run()`ner of the
callback doesn't see those values or even know that the function it's calling.

```cpp
void MyFunc(int i, const std::string& str) {}
base::RepeatingCallback<void()> cb = base::BindRepeating(&MyFunc, 23, "hello world");
cb.Run();
```

As described earlier, a callback with no unbound input parameters
(`base::RepeatingCallback<void()>`) is called a `base::RepeatingClosure`. So we
could have also written:

```cpp
base::RepeatingClosure cb = base::BindRepeating(&MyFunc, 23, "hello world");
```

When calling member functions, bound parameters just go after the object
pointer.

```cpp
base::RepeatingClosure cb = base::BindRepeating(&MyClass::MyFunc, this, 23, "hello world");
```

### Partial Binding Of Parameters

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

This technique is known as [partial
application](http://en.wikipedia.org/wiki/Partial_application). It should be
used in lieu of creating an adapter class that holds the bound arguments. Notice
also that the `"MyPrefix: "` argument is actually a `const char*`, while
`DisplayIntWithPrefix` actually wants a `const std::string&`. Like normal
function dispatch, `base::Bind`, will coerce parameter types if possible.

### Avoiding Copies With Callback Parameters

A parameter of `base::BindRepeating()` or `base::BindOnce()` is moved into its
internal storage if it is passed as a rvalue.

```cpp
std::vector<int> v = {1, 2, 3};
// |v| is moved into the internal storage without copy.
base::BindOnce(&Foo, std::move(v));
```

```cpp
// The vector is moved into the internal storage without copy.
base::BindOnce(&Foo, std::vector<int>({1, 2, 3}));
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

// |p| is moved into the internal storage of BindOnce(), and moved out to |Foo|.
base::BindOnce(&Foo, std::move(p));
base::BindRepeating(&Foo, base::Passed(&p)); // Ok, but subtle.
base::BindRepeating(&Foo, base::Passed(std::move(p))); // Ok, but subtle.
```

## Quick reference for advanced binding

### Binding A Class Method With Weak Pointers

Callbacks to a class method may be bound using a weak pointer as the receiver.
A callback bound using a weak pointer receiver will be automatically cancelled
(calling `Run()` becomes a no-op) if the weak pointer is invalidated, e.g. its
associated class instance is destroyed.

The most common way to use this pattern is by embedding a `base::WeakPtrFactory`
field, e.g.:

```cpp
class MyClass {
 public:
  MyClass();

  void Foo();

 private:
  std::string data_;

  // Chrome's compiler toolchain enforces that any `WeakPtrFactory`
  // fields are declared last, to avoid destruction ordering issues.
  base::WeakPtrFactory<MyClass> weak_factory_{this};
};
```

Then use `base::WeakPtrFactory<T>::GetWeakPtr()` as the receiver when
binding a callback:

```cpp
base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
    FROM_HERE,
    base::BindOnce(&MyClass::Foo, weak_factory_.GetWeakPtr());
```

If `this` is destroyed before the posted callback runs, the callback will
simply become a no-op when run, rather than being a use-after-free bug on
the destroyed `MyClass` instance.

**Sequence safety**

Class method callbacks bound to `base::WeakPtr`s must be run on the same
sequence on which the object will be destroyed to avoid potential races
between object destruction and callback execution. The same caveat applies if
a class manually invalidates live `base::WeakPtr`s with
`base::WeakPtrFactory<T>::InvalidateWeakPtrs()`.

### Binding A Class Method With Manual Lifetime Management

If a callback bound to a class method does not need cancel-on-destroy
semantics (because there is some external guarantee that the class instance will
always be live when running the callback), then use:

```cpp
// base::Unretained() is safe since `this` joins `background_thread_` in the
// destructor.
background_thread_->PostTask(
    FROM_HERE, base::BindOnce(&MyClass::Foo, base::Unretained(this)));
```

It is often a good idea to add a brief comment to explain why
`base::Unretained()` is safe in this context; if nothing else, for future code
archaeologists trying to fix a use-after-free bug.

An alternative is `base::WeakPtrFactory<T>::GetSafeRef()`:

```cpp
background_thread_->PostTask(
    FROM_HERE, base::BindOnce(&MyClass::Foo, weak_factory_.GetSafeRef());
```

Similar to `base::Unretained()`, this disables cancel-on-destroy semantics;
unlike `base::Unretained()`, this is guaranteed to terminate safely if the
lifetime expectations are violated.

### Binding A Class Method And Having The Callback Own The Class

```cpp
MyClass* myclass = new MyClass;
base::BindOnce(&MyClass::Foo, base::Owned(myclass));
```

The object will be deleted when the callback is destroyed, even if it's not run
(like if you post a task during shutdown). Potentially useful for "fire and
forget" cases.

Smart pointers (e.g. `std::unique_ptr<>`) are also supported as the receiver.

```cpp
std::unique_ptr<MyClass> myclass(new MyClass);
base::BindOnce(&MyClass::Foo, std::move(myclass));
```

### Ignoring Return Values

Sometimes you want to call a function that returns a value in a callback that
doesn't expect a return value.

```cpp
int DoSomething(int arg) {
  cout << arg << endl;
  return arg;
}
base::RepeatingCallback<void(int)> cb =
    base::BindRepeating(IgnoreResult(&DoSomething));
```

Similarly, you may want to use an existing callback that returns a value in a
place that expects a void return type.

```cpp
base::RepeatingCallback<int()> cb = base::BindRepeating([](){ return 5; });
base::RepeatingClosure void_cb = base::BindRepeating(base::IgnoreResult(cb));
```

### Ignoring Arguments Values

Sometimes you want to use a function that takes fewer arguments than the
designated callback type expects. The extra arguments can be ignored as long
as they are leading.

```cpp
bool LogError(char* error_message) {
  if (error_message) {
    cout << "Log: " << error_message << endl;
    return false;
  }
  return true;
}
base::RepeatingCallback<bool(int, char*)> cb =
    base::IgnoreArgs<int>(base::BindRepeating(&LogError));
CHECK_EQ(true, cb.Run(42, nullptr));
```

Note in the example above that the type(s) passed to `IgnoreArgs` represent
the additional prepended parameters (those which will be "ignored"). The other
arguments to `cb` are inferred from the callback that is being wrapped.

`IgnoreArgs` can be used to adapt a closure to a callback, ignoring all the
arguments that are eventually passed:

```cpp
base::OnceClosure closure = base::BindOnce([](){ cout << "Hello!" << endl; });
base::OnceCallback<void(int)> int_cb =
    base::IgnoreArgs<int>(std::move(closure));
```

## Quick reference for binding parameters to BindOnce() and BindRepeating()

Bound parameters are specified as arguments to `base::Bind{Once, Repeating}()`
and are passed to the functions.

### Passing Parameters Owned By The Callback

```cpp
void Foo(int* arg) { cout << *arg << endl; }
int* pn = new int(1);
base::RepeatingClosure foo_callback = base::BindRepeating(&foo, base::Owned(pn));
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
base::RepeatingClosure cb = base::BindRepeating(&TakesOneRef, f);
```

This should "just work." The closure will take a reference as long as it is
alive, and another reference will be taken for the called function.

```cpp
void DontTakeRef(Foo* arg) {}
scoped_refptr<Foo> f(new Foo);
base::RepeatingClosure cb = base::BindRepeating(&DontTakeRef, base::RetainedRef(f));
```

`base::RetainedRef` holds a reference to the object and passes a raw pointer to
the object when the Callback is run.

### Binding Const Reference Parameters

If the callback function takes a const reference parameter then the value is
*copied* when bound unless `std::ref` or `std::cref` is used. Example:

```cpp
void foo(const int& arg) { printf("%d %p\n", arg, &arg); }
int n = 1;
base::OnceClosure has_copy = base::BindOnce(&foo, n);
base::OnceClosure has_ref = base::BindOnce(&foo, std::cref(n));
n = 2;
foo(n);                                   // Prints "2 0xaaaaaaaaaaaa"
std::move(has_copy).Run();                // Prints "1 0xbbbbbbbbbbbb"
std::move(has_ref).Run();                 // Prints "2 0xaaaaaaaaaaaa"
```

Normally parameters are copied in the closure. **DANGER**: `std::ref` and
`std::cref` store a (const) reference instead, referencing the original
parameter. This means that you must ensure the object outlives the callback!

### Binding Non-Const Reference Parameters

If the callback function takes a non-const reference then the bind statement
must specify what behavior is desired. If a reference that can mutate the
original value is desired then `std::ref` is used. If the callback should take
ownership of the value, either by making a copy or moving an existing value,
then `base::OwnedRef` is used. If neither is used the bind statement will fail
to compile. Example:

```cpp
void foo(int& arg) {
  printf("%d\n", arg);
  ++arg;
}

int n = 0;
base::RepeatingClosure has_ref = base::BindRepeating(&foo, std::ref(n));
base::RepeatingClosure has_copy = base::BindRepeating(&foo, base::OwnedRef(n));

foo(n);                        // Prints "0"
has_ref.Run();                 // Prints "1"
has_ref.Run();                 // Prints "2"
foo(n);                        // Prints "3"

has_copy.Run();                // Prints "0"
has_copy.Run();                // Prints "1"

// This will fail to compile.
base::RepeatingClosure cb = base::BindRepeating(&foo, n);
```

Normally parameters are copied in the closure. **DANGER**: `std::ref` stores a
reference instead, referencing the original parameter. This means that you must
ensure the object outlives the callback!

If the callback function has an output reference parameter but the output value
isn't needed then `base::OwnedRef()` is a convenient way to handle it. The
callback owned value will be mutated by the callback function and then deleted
along with the callback. Example:

```cpp
bool Compute(size_t index, int& output);

// The `output` parameter isn't important for the callback, it only cares about
// the return value.
base::OnceClosure cb = base::BindOnce(&Compute, index, base::OwnedRef(0));
bool success = std::move(cb).Run();
```

## Implementation notes

### Where Is This Design From:

The design is heavily influenced by C++'s `tr1::function` / `tr1::bind`, and by
the "Google Callback" system used inside Google.

### Customizing the behavior

There are several injection points that controls binding behavior from outside
of its implementation.

```cpp
namespace base {

template <typename Receiver>
struct IsWeakReceiver : std::false_type {};

template <typename Obj>
struct BindUnwrapTraits {
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

`base::BindUnwrapTraits<BoundObject>::Unwrap()` is called for each bound argument
right before the callback calls the target function. You can specialize this to
define an argument wrapper such as `base::Unretained`, `base::Owned`,
`base::RetainedRef` and `base::Passed`.

### How The Implementation Works:

There are three main components to the system:
  1) The `base::{Once, Repeating}Callback<>` classes.
  2) The `base::BindOnce() and base::BindRepeating()` functions.
  3) The arguments wrappers (e.g., `base::Unretained()` and `base::Owned()`).

The Callback classes represent a generic function pointer. Internally, it
stores a refcounted piece of state that represents the target function and all
its bound parameters. The `base::{Once, Repeating}Callback` constructor takes a
`base::BindStateBase*`, which is upcasted from a `base::BindState<>`. In the
context of the constructor, the static type of this `base::BindState<>` pointer
uniquely identifies the function it is representing, all its bound parameters,
and a `Run()` method that is capable of invoking the target.

base::BindOnce() or base::BindRepeating() creates the `base::BindState<>` that
has the full static type, and erases the target function type as well as the
types of the bound parameters. It does this by storing a pointer to the specific
`Run()` function, and upcasting the state of `base::BindState<>*` to a
`base::BindStateBase*`.  This is safe as long as this `BindStateBase` pointer is
only used with the stored `Run()` pointer.

These bind functions, along with a set of internal templates, are responsible
for

 - Unwrapping the function signature into return type, and parameters
 - Determining the number of parameters that are bound
 - Creating the BindState storing the bound parameters
 - Performing compile-time asserts to avoid error-prone behavior
 - Returning a `Callback<>` with an arity matching the number of unbound
   parameters and that knows the correct refcounting semantics for the
   target object if we are binding a method.

The `base::Bind` functions do the above using type-inference and variadic
templates.

By default `base::Bind{Once, Repeating}()` will store copies of all bound parameters, and
attempt to refcount a target object if the function being bound is a class
method. These copies are created even if the function takes parameters as const
references. (Binding to non-const references is forbidden, see bind.h.)

To change this behavior, we introduce a set of argument wrappers (e.g.,
`base::Unretained()`).  These are simple container templates that are passed by
value, and wrap a pointer to argument.  Each helper has a comment describing it
in base/functional/bind.h.

These types are passed to the `Unwrap()` functions to modify the behavior of
`base::Bind{Once, Repeating}()`.  The `Unwrap()` functions change behavior by doing partial
specialization based on whether or not a parameter is a wrapper type.

`base::Unretained()` is specific to Chromium.

### Missing Functionality
 - Binding arrays to functions that take a non-const pointer.
   Example:
```cpp
void Foo(const char* ptr);
void Bar(char* ptr);
base::BindOnce(&Foo, "test");
base::BindOnce(&Bar, "test");  // This fails because ptr is not const.
```
 - In case of partial binding of parameters a possibility of having unbound
   parameters before bound parameters. Example:
```cpp
void Foo(int x, bool y);
base::BindOnce(&Foo, _1, false); // _1 is a placeholder.
```

If you are thinking of forward declaring `base::{Once, Repeating}Callback` in
your own header file, please include "base/functional/callback_forward.h"
instead.
