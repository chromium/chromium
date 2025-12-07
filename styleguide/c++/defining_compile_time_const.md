# Defining compile-time constants correctly

Using the wrong specifiers on compile-time constants can have surprising effects. For example, Chrome contains many variables that are [declared `constexpr` in a header file but omit `inline`](https://source.chromium.org/search?q=%5Econstexpr%5C%20%5B%5Ei%5D%5B%5E\(\)%3D%5D%2B%5C%20%3D%20%20file:%5C.h$%20-file:%5Egen%2F%20-file:%5Eout%2F%20-file:%5Esrc%2F%20-file:third_party%2F\(%5B%5Eb%5D%7Cb%5B%5El%5D%7Cbl%5B%5Ei%5D%7Cbli%5B%5En%5D%7Cblin%5B%5Ek%5D%7Cblink%5B%5E%2F%5D\)). This gives every `.cc` file using these constants its own copies, which violates the one-definition rule (“ODR”) and is technically undefined behavior (“UB”).

*** note
**Note:** In this case, the likely effect is duplicated symbols for some such constants, most commonly arrays. ODR violations with non-`constexpr` variables often have more subtle and dangerous effects.
***

# Guidelines

1. **Define constants [in the narrowest scope possible](https://google.github.io/styleguide/cppguide.html#Local_Variables)**, just like with other variables. This minimizes name conflicts, symbol duplication, and dead code.
   * Don’t put things at namespace scope or in headers unless you actually need to refer to them from multiple places. “There are other constants atop this file” and “other code in Chromium does this” are not sufficient justification.
2. **Use `constexpr`, not `const`** (or `#define` or the “`enum` hack”), to declare a compile-time constant whenever possible. This clarifies intent and helps the optimizer avoid unnecessary codegen.=
   * If the type in question has no `constexpr` constructor, can one be added?
3. **Use `static constexpr` for constants in functions and classes.**
   * The meaning and importance of `static` is different in these cases, but using this as the common pattern is easy to remember.
4. **Use `constexpr` inside an anonymous namespace for file-scope constants in `.cc` files.**
5. **Use `inline constexpr` for file-scope constants in `.h` files.**
   * Avoid `extern const` declarations; these hide constants’ values from the compiler, preventing optimizations, and often result in extra `.cc` files just to define constants. (Such constants also often forget to use `constexpr` or `constinit`.)
     * Even in component builds, a non-exported `inline constexpr` is preferable to an exported `extern const`.

# Details

At **function scope**, make constants `static` to [help the optimizer avoid unnecessary codegen](https://stackoverflow.com/a/76696023).

*** note
**Note:** We [currently pass `-fmerge-all-constants`](https://source.chromium.org/chromium/chromium/src/+/main:build/config/compiler/BUILD.gn;l=541;drc=9604abef2d2ec468f7553abcf567129c5a34ec5e), which reduces the risk of this, but we [might not always do so](https://crbug.com/40570474).
***


```cxx
// Do this.
void Good() {
  static constexpr int kA = 1;         // Will be optimized away.
  ...
}

// Don't do these.
void Bad() {
  int kA = 1;                          // Not a compile-time constant.
  const int kB = 2;                    // Not a compile-time constant.
  constexpr int kC = 3;                // Might not be optimized away if ODR-used.
  inline constexpr int kD = 4;         // Won't compile.
  static inline constexpr int kE = 5;  // Won't compile.
  ...
}
```

At **class scope**, constants must be `static`, because making them non-`static` asks the compiler to give each instance its own copy, which isn’t sensible for compile-time constants.

```cxx
// Do this.
class Good {
 public:
  static constexpr int kA = 1;         // Will be optimized away.
  ...
};

// Don't do these.
class Bad {
 public:
  int kA = 1;                          // Normal member, not a compile-time constant.
  const int kB = 2;                    // Normal member, not a compile-time constant.
  static int kC = 3;                   // Not a compile-time constant.
  static const int kD = 4;             // Not a compile-time constant.
  constexpr int kE = 5;                // Won't compile.
  inline constexpr int kF = 6;         // Won't compile.
  static inline constexpr int kG = 7;  // Unnecessarily verbose, inline is
                                       // implicit to static in this case.
  ...
};
```

At **namespace scope**, the rules differ for `.cc` and `.h` files because the former will only be compiled into a single object, while the latter will potentially be compiled as part of many objects. For `.cc` files, place constants in an unnamed namespace [to ensure they have internal linkage](https://eel.is/c++draft/basic.link#4); this avoids name conflicts and helps detect unused constants (and [complies with the style guide](https://google.github.io/styleguide/cppguide.html#Internal_Linkage)).

```cxx
// In foo.cc
...
// Do this.
namespace {
constexpr int kA = 1;         // Will be optimized away.
}

// Don't do these.
constexpr int kA = 1;         // Could have external linkage in some scenarios.
static constexpr int kB = 2;  // Legal, but less common; overloaded meaning of `static`
                              // can confuse some people.
namespace {
int kC = 3;                   // Not a compile-time constant.
const int kD = 4;             // Not guaranteed to be compile-time constant.
inline constexpr int kE = 5;  // `inline` unnecessary, since constant is unique.
static constexpr int kF = 6;  // `static` redundant.
enum : int { kG = 7 };        // Cryptic, and no longer necessary as of C++17.
}
...
```

For `.h` files, constants [should have external linkage](https://google.github.io/styleguide/cppguide.html#Internal_Linkage) (or why are they at namespace scope in a header?). In this case, use `inline` so the linker will fold duplicate definitions into one.

*** note
**Note:** For functions, `constexpr` implies `inline`. For variables, it does not. This is not confusing.
***

```cxx
// In foo.h
...
// Do this.
inline constexpr int kA = 1;  // Will result in a single, read-only definition.

// Don't do these.
constexpr int kA = 1;         // Usually results in ODR violations.
static constexpr int kB = 2;  // Has internal linkage.
inline int kC = 3;            // Not a compile-time constant.
inline const int kD = 4;      // Not guaranteed to be compile-time constant.
namespace {
constexpr int kE = 5;         // Has internal linkage.
inline constexpr int kF = 6;  // Has internal linkage.
}
#define kG 7                  // More chance of compile errors and bugs.
...
```

## String constants

There are effectively two ways to define a compile-time string constant: as a character array, or as a view type (`std::string_view`, etc.). Use the following heuristics:

* If code needs to get the string length, e.g. via `sizeof(s) - 1`, `std::size(s) - 1`, or `strlen(s)`, **use a view type**, since this information is baked-in and you’re less likely to make an off-by-one error.
  * If you will ever access the underlying data as a C-style string (i.e. call `.data()`), use `base::cstring_view` to ensure the source character array is null-terminated and the returned string pointer will be also.
  * Otherwise, use `std::string_view`, since that’s directly consumable by more APIs and semantically indicates you don’t care about the trailing nul.
* To initialize a `std::string`, to use embedded null bytes, or otherwise, **default to a character array**, since this won’t risk generating [relocations](https://en.wikipedia.org/wiki/Relocation_\(computing\)).

Beyond the choice of type, the guidelines above apply. In particular, don’t use `extern const` string constant declarations in headers, even if you see existing code doing so; besides being more verbose and potentially less well-optimized, this can hide compiler warnings about runtime encoding conversions of compile-time string constants, which should be fixed instead. (If it’s convenient, you’re welcome to convert existing `extern const`s to `inline constexpr`s.)

```cxx
// In foo.h
...
// Do this.
inline constexpr char kA[] = "ABC";              // By default.
inline constexpr std::string_view kB = "DEF";    // If you need the length.
inline constexpr base::cstring_view kC = "GHI";  // If you need the length, and will ever
                                                 // call `.data()`.

// Don't do these.
extern const char kA[];                          // More verbose and poorly-optimized.
inline constexpr const char kB[] = "ABC";        // `const` is redundant.
inline constexpr char* kC = "DEF";               // May not compile.
inline constexpr const char* kD = "GHI";         // Can't get string length at compile
                                                 // time via array size.
inline constexpr std::string_view kE = "JKL";    // If you don't need the length: may
                                                 // generate a relocation if ODR-used,
                                                 // increasing binary size and load time.
inline constexpr std::string kF = "MNO";         // Accidentally works for short strings.
                                                 // Not portable.
...
```

# Further reading

While slightly dated, [Abseil Tip of the Week \#140](https://abseil.io/tips/140) also discusses this subject.
