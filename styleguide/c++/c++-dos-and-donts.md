# C++ Dos and Don'ts

## A Note About Usage

Unlike the style guide, the content of this page is advisory, not required. You
can always deviate from something on this page, if the relevant
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

## Variable initialization

There are myriad ways to initialize variables in C++11.  Prefer the following
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
   std::vector<double> v(500, 0.97);  // Creates 50 copies of the provided initializer
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
[`base::MakeRefCounted<T>(...)`](https://cs.chromium.org/chromium/src/base/memory/scoped_refptr.h?q=MakeRefCounted):

```cpp
// BAD: bare call to new; for refcounted types, not compatible with one-based
// refcounting.
return base::WrapUnique(new T(1, 2, 3));
return base::WrapRefCounted(new T(1, 2, 3));

// BAD: same as the above, plus mentions type names twice.
std::unique_ptr<T> t(new T(1, 2, 3));
base::scoped_refptr<T> t(new T(1, 2, 3));
return std::unique_ptr<T>(new T(1, 2, 3));
return base::scoped_refptr<T>(new T(1, 2, 3));

// OK, but verbose: type name still mentioned twice.
std::unique_ptr<T> t = std::make_unique<T>(1, 2, 3);
base::scoped_refptr<T> t = base::MakeRefCounted<T>(1, 2, 3);

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
auto item = new Item();  // BAD: auto deduces to Item*, type of |item| is Item*
auto* item = new Item(); // GOOD: auto deduces to Item, type of |item| is Item*
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

The common ways to represent names in comments are as follows:
* Class and type names: `FooClass`
* Function name: `FooFunction()`. The trailing parens disambiguate against
  class names, and, occasionally, English words.
* Variable name: `|foo_var|`. Again, the vertical lines disambiguate against
  English words, and, occasionally, inlined function names. Code search will
  also automatically convert `|foo_var|` into a clickable link.

```cpp
// FooImpl implements the FooBase class.
// FooFunction() modifies |foo_member_|.
```

## Named namespaces

Named namespaces are discouraged in top-level embedders (e.g., `chrome/`). See
[this thread](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/8ROncnL1t4k/J7uJMCQ8BwAJ)
for background and discussion.
