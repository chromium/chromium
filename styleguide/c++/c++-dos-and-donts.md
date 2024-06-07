# C++ Dos and Don'ts

## A Note About Usage

Unlike the [style guide](c++.md), the content of this page is advisory, not
required. You can always deviate from something on this page, if the relevant
author/reviewer/OWNERS agree that another course is better.

## Minimize Code in Headers

* Remove #includes you don't use.  Unfortunately, Chromium lacks
  include-what-you-use ("IWYU") support, so there's no tooling to do this
  automatically.  Look carefully when refactoring.
* Where possible, forward-declare nested classes, then give the full declaration
  (and definition) in the .cc file.
* Defining a class method in the declaration is an implicit request to inline
  it.  Avoid this in header files except for cheap non-virtual getters and
  setters.  Note that constructors and destructors can be more expensive than
  they appear and should also generally not be inlined.

## Static variables

Dynamic initialization of function-scope static variables is **thread-safe** in
Chromium (per standard C++11 behavior). Before 2017, this was thread-unsafe, and
base::LazyInstance was widely used. This is no longer necessary.
Background can be found in
[this thread](https://groups.google.com/a/chromium.org/forum/#!msg/chromium-dev/p6h3HC8Wro4/HHBMg7fYiMYJ)
and
[this thread](https://groups.google.com/a/chromium.org/d/topic/cxx/j5rFewBzSBQ/discussion).

```cpp
void foo() {
  static int ok_count = ComputeTheCount();  // OK; a problem pre-2017.
  static int good_count = 42;               // Done before dynamic initialization.
  static constexpr int better_count = 42;   // Even better (likely inlined at compile time).
  static auto& object = *new Object;        // For class types.
}
```

## Explicitly declare class copyability/movability

The
[Google Style Guide](http://google.github.io/styleguide/cppguide.html#Copyable_Movable_Types)
says classes can omit copy/move declarations or deletions "only if they are
obvious".  Because "obvious" is subjective and even the examples in the style
guide take some thought to figure out, being explicit is clear, simple, and
avoids any risk of accidental copying.

Declare or delete these operations in the public section, between other
constructors and the destructor; `DISALLOW_COPY_AND_ASSIGN` is deprecated.  For
a non-copyable/movable type, delete the copy operations (the move operations
will be implicitly deleted); otherwise, declare either copy operations, move
operations, or both (a non-declared pair will be implicitly deleted).  Always
declare or delete both construction and assignment, not just one (which can
introduce subtle bugs).

```cpp
class TypeName {
 public:
  TypeName(int arg);
  ...
  TypeName(const TypeName&) = delete;
  TypeName& operator=(const TypeName&) = delete;
  ...
  ~TypeName();
}
```

## Variable initialization

There are myriad ways to initialize variables in C++. Prefer the following
general rules:

1. Use assignment syntax when performing "simple" initialization with one or
   more literal values which will simply be composed into the object:

   ```cpp
   int i = 1;
   std::string s = "Hello";
   std::pair<bool, double> p = {true, 2.0};
   std::vector<std::string> v = {"one", "two", "three"};
   ```

   Using '=' here is no less efficient than "()" (the compiler won't generate a
   temp + copy), and ensures that only implicit constructors are called, so
   readers seeing this syntax can assume    nothing complex or subtle is
   happening.  Note that "{}" are allowed on the right side of the '=' here
   (e.g. when you're merely passing a set of initial values to a "simple"
   struct/   container constructor; see below items for contrast).
2. Use constructor syntax when construction performs significant logic, uses an
   explicit constructor, or in some other way is not intuitively "simple" to the
   reader:

   ```cpp
   MyClass c(1.7, false, "test");
   std::vector<double> v(500, 0.97);  // Creates 500 copies of the provided initializer
   ```
3. Use C++11 "uniform init" syntax ("{}" without '=') only when neither of the
   above work:

   ```cpp
   class C {
    public:
     explicit C(bool b) { ... };
     ...
   };
   class UsesC {
     ...
    private:
     C c{true};  // Cannot use '=' since C() is explicit (and "()" is invalid syntax here)
   };
   class Vexing {
    public:
     explicit Vexing(const std::string& s) { ... };
     ...
   };
   void func() {
     Vexing v{std::string()};  // Using "()" here triggers "most vexing parse";
                               // "{}" is arguably more readable than "(())"
     ...
   }
   ```
4. Never mix uniform init syntax with auto, since what it deduces is unlikely
   to be what was intended:

   ```cpp
   auto x{1};  // Until C++17, decltype(x) is std::initializer_list<int>, not int!
   ```

For more reading, please see abseil's [Tip of the Week #88: Initialization: =,
(), and {}](https://abseil.io/tips/88).

## Initialize members in the declaration where possible

If possible, initialize class members in their declarations, except where a
member's value is explicitly set by every constructor.

This reduces the chance of uninitialized variables, documents default values in
the declaration, and increases the number of constructors that can use
`=default` (see below).

```cpp
class C {
 public:
  C() : a_(2) {}
  C(int b) : a_(1), b_(b) {}

 private:
  int a_;          // Not necessary to init this since all constructors set it.
  int b_ = 0;      // Not all constructors set this.
  std::string c_;  // No initializer needed due to string's default constructor.
  base::WeakPtrFactory<C> factory_{this};
                   // {} allows calling of explicit constructors.
};
```

Note that it's possible to call functions or pass `this` and other expressions
in initializers, so even some complex initializations can be done in the
declaration.

## Use `std::make_unique` and `base::MakeRefCounted` instead of bare `new`

When possible, avoid bare `new` by using
[`std::make_unique<T>(...)`](http://en.cppreference.com/w/cpp/memory/unique_ptr/make_unique)
and
[`base::MakeRefCounted<T>(...)`](https://source.chromium.org/chromium/chromium/src/+/main:base/memory/scoped_refptr.h;l=98;drc=f8c5bd9d40969f02ddeb3e6c7bdb83029a99ca63):

```cpp
// BAD: bare call to new; for refcounted types, not compatible with one-based
// refcounting.
return base::WrapUnique(new T(1, 2, 3));
return base::WrapRefCounted(new T(1, 2, 3));

// BAD: same as the above, plus mentions type names twice.
std::unique_ptr<T> t(new T(1, 2, 3));
scoped_refptr<T> t(new T(1, 2, 3));
return std::unique_ptr<T>(new T(1, 2, 3));
return scoped_refptr<T>(new T(1, 2, 3));

// OK, but verbose: type name still mentioned twice.
std::unique_ptr<T> t = std::make_unique<T>(1, 2, 3);
scoped_refptr<T> t = base::MakeRefCounted<T>(1, 2, 3);

// GOOD; make_unique<>/MakeRefCounted<> are clear enough indicators of the
// returned type.
auto t = std::make_unique<T>(1, 2, 3);
auto t = base::MakeRefCounted<T>(1, 2, 3);
return std::make_unique<T>(1, 2, 3);
return base::MakeRefCounted<T>(1, 2, 3);
```

**Notes:**

1. Never friend `std::make_unique` to work around constructor access
   restrictions. It will allow anyone to construct the class. Use
   `base::WrapUnique` in this case.

   DON'T:
   ```cpp
   class Bad {
    public:
     std::unique_ptr<Bad> Create() { return std::make_unique<Bad>(); }
     // ...
    private:
     Bad();
     // ...
     friend std::unique_ptr<Bad> std::make_unique<Bad>();  // Lost access control
   };
   ```

   DO:
   ```cpp
   class Okay {
    public:
     // For explanatory purposes. If Create() adds no value, it is better just
     // to have a public constructor instead.
     std::unique_ptr<Okay> Create() { return base::WrapUnique(new Okay()); }
     // ...
    private:
     Okay();
     // ...
   };
   ```
2. `base::WrapUnique(new Foo)` and `base::WrapUnique(new Foo())` mean something
   different if `Foo` does not have a user-defined constructor. Don't make
   future maintainers guess whether you left off the '()' on purpose. Use
   `std::make_unique<Foo>()` instead. If you're intentionally leaving off the
   "()" as an optimization, please leave a comment.

   ```cpp
   auto a = base::WrapUnique(new A); // BAD: "()" omitted intentionally?
   auto a = std::make_unique<A>();   // GOOD
   // "()" intentionally omitted to avoid unnecessary zero-initialization.
   // base::WrapUnique() does the wrong thing for array pointers.
   auto array = std::unique_ptr<A[]>(new A[size]);
   ```

See also [TOTW 126](https://abseil.io/tips/126).

## Do not use `auto` to deduce a raw pointer

Do not use `auto` when the type would be deduced to be a pointer type; this can
cause confusion. Instead, specify the "pointer" part outside of `auto`:

```cpp
auto item = new Item();  // BAD: auto deduces to Item*, type of `item` is Item*
auto* item = new Item(); // GOOD: auto deduces to Item, type of `item` is Item*
```

## Use `const` correctly

For safety and simplicity, **don't return pointers or references to non-const
objects from const methods**. Within that constraint, **mark methods as const
where possible**.  **Avoid `const_cast` to remove const**, except when
implementing non-const getters in terms of const getters.

For more information, see [Using Const Correctly](const.md).

## Prefer to use `=default`

Use `=default` to define special member functions where possible, even if the
default implementation is just {}. Be careful when defaulting move operations.
Moved-from objects must be in a valid but unspecified state, i.e., they must
satisfy the class invariants, and the default implementations may not achieve
this.

```cpp
class Good {
 public:
  // We can, and usually should, provide the default implementation separately
  // from the declaration.
  Good();

  // Use =default here for consistency, even though the implementation is {}.
  ~Good() = default;
  Good(const Good& other) = default;

 private:
  std::vector<int> v_;
};

Good::Good() = default;
```

## Comment style

References to code in comments should be wrapped in `` ` ` `` pairs. Codesearch
uses this as a heuristic for finding C++ symbols in comments and generating
cross-references for that symbol. Historically, Chrome also used `||` pairs to
delimit variable names; codesearch understands both conventions and will
generate a cross-reference either way. Going forward, prefer the new style even
if existing code uses the old one.

* Class and type names: `` `FooClass` ``.
* Function names: `` `FooFunction()` ``. The trailing parens disambiguate
  against class names, and occasionally, English words.
* Variable names: `` `foo_var` ``.
* Tracking comments for future improvements: `// TODO(crbug.com/40781525): ...`,
  or, less optimally, `// TODO(knowledgeable_username): ...`.  Tracking bugs
  provide space to give background context and current status; a username might
  at least provide a starting point for asking about an issue.

```cpp
// `FooImpl` implements the `FooBase` class.
// `FooFunction()` modifies `foo_member_`.
// TODO(crbug.com/40097047): Rename things to something more descriptive than "foo".
```

## Named namespaces

Most code should be in a namespace, with the exception of code under
`//chrome`, which may be in the global namespace (do not use the `chrome::`
namespace). Minimize use of nested namespaces, as they do not actually
improve encapsulation; if a nested namespace is needed, do not reuse the
name of any top-level namespace. For more detailed guidance and rationale,
see https://abseil.io/tips/130.

## Guarding with DCHECK_IS_ON()

Any code written inside a `DCHECK()` macro, or the various `DCHECK_EQ()` and
similar macros, will be compiled out in builds where DCHECKs are disabled. That
includes any non-debug build where the `dcheck_always_on` GN arg is not present.

Thus even if your `DHECK()` would perform some expensive operation, you can
be confident that **code within the macro will not run in our official
release builds**, and that the linker will consider any function it calls to be
dead code if it's not used elsewhere.

However, if your `DCHECK()` relies on work that is done outside of the
`DCHECK()` macro, that work may not be eliminated in official release builds.
Thus any code that is only present to support a `DCHECK()` should be guarded by
`if constexpr (DCHECK_IS_ON())` (see the next item) or `#if DCHECK_IS_ON()` to
avoid including that code in official release builds.

This code is fine without any guards for `DCHECK_IS_ON()`.
```cpp
void ExpensiveStuff() { ... }  // No problem.

// The ExpensiveStuff() call will not happen in official release builds. No need
// to use checks for DCHECK_IS_ON() anywhere.
DCHECK(ExpensiveStuff());

std::string ExpensiveDebugMessage() { ... }  // No problem.

// Calls in stream operators are also dead code in official release builds (not
// called with the result discarded). This is fine.
DCHECK(...) << ExpensiveDebugMessage();
```

This code will probably do expensive things that are not needed in official
release builds, which is bad.
```cpp
// The result of this call is only used in a DCHECK(), but the code here is
// outside of the macro. That means it's likely going to show up in official
// release builds.
int c = ExpensiveStuff();  // Bad. Don't do this.
...
DCHECK_EQ(c, ExpensiveStuff());
```

Instead, any code outside of a `DCHECK()` macro, that is only needed when
DCHECKs are enabled, should be explicitly eliminated by checking
`DCHECK_IS_ON()` as this code does.
```cpp
// The result of this call is only used in a DCHECK(), but the code here is
// outside of the macro. We can't rely on the compiler to remove this in
// official release builds, so we should guard it with a check for
// DCHECK_IS_ON().
#if DCHECK_IS_ON()
int c = ExpensiveStuff();  // Great, this will be eliminated.
#endif
...
#if DCHECK_IS_ON()
DCHECK_EQ(c, ExpensiveStuff());  // Must be guarded since `c` won't exist.
#endif
```

The `DCHECK()` and friends macros still require the variables and functions they
use to be declared at compile time, even though they will not be used at
runtime. This is done to avoid "unused variable" and "unused function" warnings
when DCHECKs are turned off. This means that you may need to guard the
`DCHECK()` macro if it depends on a variable or function that is also guarded
by a check for `DCHECK_IS_ON()`.

## Minimizing preprocessor conditionals

Eliminate uses of `#if ...` when there are reasonable alternatives. Some common
cases:

* APIs that are conceptually reasonable for all platforms, but only actually do
  anything on one. Instead of guarding the API and all callers in `#if`s, you
  can define and call the API unconditionally, and guard platform-specific
  implementation.
* Test code that expects different values under different `#define`s:
  ```cpp
    // Works, but verbose, and might be more annoying/prone to bugs during
    // future maintenance.
  #if BUILDFLAG(COOL_FEATURE)
    EXPECT_EQ(5, NumChildren());
  #else
    EXPECT_EQ(3, NumChildren());
  #endif

    // Shorter and less repetitive.
    EXPECT_EQ(BUILDFLAG(COOLFEATURE) ? 5 : 3, NumChildren());
  ```
* Code guarded by `DCHECK_IS_ON()` or a similar "should always work in either
  configuration" `#define`, which could still compile when the `#define` is
  unset. Prefer `if constexpr (DCHECK_IS_ON())` or similar, since the compiler
  will continue to verify the code's syntax in all cases, but it will not be
  compiled in if the condition is false. Note that this only works inside a
  function, and only if the code does not refer to symbols whose declarations
  are `#ifdef`ed away. Don't unconditionally declare debug-only symbols just
  to use this technique -- only use it when it doesn't require additional
  tweaks to the surrounding code.