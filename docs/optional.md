# base::Optional

`base::Optional<T>` is a container that might contain an instance of `T`.

[TOC]

## History

[base::Optional<T>](https://source.chromium.org/chromium/chromium/src/+/HEAD:base/optional.h)
is an implementation of [std::optional<T>](https://en.cppreference.com/w/cpp/utility/optional),
initially a C++ experimental feature and now part of the C++17 standard. The
Chromium's implementation is as close as possible to the specification. The
differences are listed at the beginning of the header. The most important
difference is that all the objects and types are part of the `base::` namespace
instead of `std::`. Also, following Chromium coding style, the class is named
`Optional` instead of `optional`.

## API description

For a deep API description, please have a look at [std::optional<T>](https://en.cppreference.com/w/cpp/utility/optional)
or the [Chromium implementation](https://source.chromium.org/chromium/chromium/src/+/HEAD:base/optional.h).

When initialized without a value, `base::Optional<T>` will be empty. When empty,
the `operator bool` will return `false` and `value()` should not be called. An
empty `base::Optional<T>` is equal to `base::nullopt`.

```C++
base::Optional<int> opt;
opt == true; // false
opt.value(); // illegal, will CHECK
opt == base::nullopt; // true
```

To pass an empty optional argument to another function, use `base::nullopt`
where you would otherwise have used a `nullptr`:

``` C++
OtherFunction(42, base::nullopt);  // Supply an empty optional argument
```

To avoid calling `value()` when an `base::Optional<T>` is empty, instead of
doing checks, it is possible to use `value_or()` and pass a default value:

```C++
base::Optional<int> opt;
opt.value_or(42); // will return 42
```

It is possible to initialize a `base::Optional<T>` from its constructor and
`operator=` using `T` or another `base::Optional<T>`:

```C++
base::Optional<int> opt_1 = 1; // .value() == 1
base::Optional<int> opt_2 = base::Optional<int>(2); // .value() == 2
```

All basic operators should be available on `base::Optional<T>`: it is possible
to compare a `base::Optional<T>` with another or with a `T` or
`base::nullopt`.

```C++
base::Optional<int> opt_1;
base::Optional<int> opt_2 = 2;

opt_1 == opt_2; // false
opt_1 = 1;

opt_1 <= opt_2; // true
opt_1 == 1; // true
opt_1 == base::nullopt; // false
```

`base::Optional<T>` has a helper function `base::make_optional<T&&>`:

```C++
base::Optional<int> opt = base::make_optional<int>(GetMagicNumber());
```

Finally, `base::Optional<T>` is integrated with `std::hash`, using
`std::hash<T>` if it is not empty, a default value otherwise. `.emplace()` and
`.swap()` can be used as members functions and `std::swap()` will work with two
`base::Optional<T>` objects.

## How is it implemented?

`base::Optional<T>` is implemented with a union with a `T` member. The object
doesn't behave like a pointer and doesn't do dynamic memory allocation. In
other words, it is guaranteed to have an object allocated when it is not empty.

## When to use?

A very common use case is for classes and structures that have an object not
always available, because it is early initialized or because the underlying data
structure doesn't require it.

It is common to implement such patterns with dynamically allocated pointers,
`nullptr` representing the absence of value. Other approaches involve
`std::pair<T, bool>` where bool represents whether the object is actually
present.

It can also be used for simple types, for example when a structure wants to
represent whether the user or the underlying data structure has some value
unspecified, a `base::Optional<int>` would be easier to understand than a
special value representing the lack of it. For example, using -1 as the
undefined value when the expected value can't be negative.

## When not to use?

It is recommended to not use `base::Optional<T>` as a function parameter as it
will force the callers to use `base::Optional<T>`. Instead, it is recommended to
keep using `T*` for arguments that can be omitted, with `nullptr` representing
no value. A helper, `base::OptionalOrNullptr`, is available in
[stl_util.h](https://source.chromium.org/chromium/chromium/src/+/HEAD:base/stl_util.h)
and can make it easier to convert `base::Optional<T>` to `T*`.

Furthermore, depending on `T`, MSVC might fail to compile code using
`base::Optional<T>` as a parameter because of memory alignment issues.
