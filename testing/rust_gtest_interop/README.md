# Rust integration into C++ Gtest targets.

This directory contains the tools for writing gtest-based tests in Rust and
integrating them into Chromium's C++ gtest binaries. The tools are all
accessible through the `rust_gtest_interop` target which is automatically
included in test targets that depend on `//testing/gtest`.

## To add rust unittests to a C++ Gtest target

A typical Gtest target is defined in a BUILD.gn file, with something like this:

`BUILD.gn`:
```gn
test("some_unittests") {
  sources = [
    "a_cpp_file.cc",
    "another_cpp_file.cc",
  ]
  deps = [
    "//testing/gtest",
  ]
}
```

To add a Rust file to the test suite, simply add it to the `rs_sources`. Unlike
other Rust crates, the `crate_root` is not specified, since it is generated from
the sources list.

`BUILD.gn`:
```gn
test("some_unittests") {
  sources = [
    "a_cpp_file.cc",
    "another_cpp_file.cc",
  ]
  rs_sources = [
    "a_rust_file.rs",
  ]
  deps = [
    "//testing/gtest",
  ]
}
```

## To write a Gtest unit test in Rust

To write a unit test, you simply write a function an decorate it with the
`#[gtest]` macro. The macro takes 2 arguments, which are the test suite name and
the test name, just like the C++ `TEST()` macro.

The `#[gtest]` macro is provided by the `rust_gtest_interop_rs` crate, and is
exported in the `prelude` module. Typically a unit test file would start with
`use rust_gtest_interop_rs::prelude::*;` which includes all of the available
gtest macros. This is similar to writing `#include
"testing/gtest/include/gtest/gtest.h"` in C++.

A Rust test:
```rs
use rust_gtest_interop_rs::prelude::*;  // Provides all the gtest macros.

#[gtest(MyTestSuite, MyTestOfThing)]
fn test() {
  ...
}
```

A C++ test:
```cpp
#include "testing/gtest/include/gtest/gtest.h"  // Provides all the gtest macros.

TEST(MyTestSuite, MyTestOfThing) {
  ...
}
```

### Expectations

We have access to many of the same EXPECT macros in Rust that are familiar to
C++ Gtest users, though they are used with Rust's macro syntax.

The macros currently available are:
```rs
expect_true!(is_friday());
expect_false!(is_saturday());

expect_eq!(2, 1 + 1);  // A == B
expect_ne!(3, 1 + 2);  // A != B

expect_lt!(1 * 1, 1 * 2);  // A < B
expect_gt!(4 * 1, 1 * 2);  // A > B
expect_le!(2 * 1, 1 * 2);  // A <= B
expect_ge!(3 * 1, 2 * 3);  // A >= B
```

### Returning a Result

A C++ test always returns void and Rust tests usually do as well. But if your
test calls a function that returns `Result`, it is convenient to make use of the
[`?` operator](https://doc.rust-lang.org/reference/expressions/operator-expr.html#the-question-mark-operator)
instead of checking the Result value explicitly. Thus a test can either return:

1. `()` aka void.
1. `std::result::Result<(), E>` for any `E` that can be converted to a
   `std::error::Error`. (Or in Rust parlance, for any `E` for which there is
   `Into<std::error::Error>`). Common error types are `std::io::Error` or
   `String`.

If the test with a `std::result::Result` return type returns `Result::Err`, the
test will fail and display the error.

In this example, the test will fail if it can not read from `file.txt`, or if it
does not contain `"hello world"`:
```rs
#[gtest(TestingIO, ReadFile)]
fn test() -> std::io::Result {
  let s = std::fs::read_to_string("file.txt")?;
  expect_eq!(s, "hello world");
  Ok(())
}
```

### Shared helper utilities

Sometimes tests across different test files want to share helper utilities. Such
helpers should be placed in a separate GN target, typically named with a
`_test_support` suffix, such as `starship_test_support` for the
`starship_unittests`. And would also usually be found in a `test/` subdirectory.

#### Example
The `starship_unittests` test() target would include any unit test files, such as
`starship_unittest.rs`. And the `starship_test_support` static_library() target
would include the files in the `test/` subdirectory, such as
`starship_test_helper.rs` and `starship_test_things.rs`.
```
src/
  starship/
    starship_unittest.rs
    test/
      starship_test_helper.rs
      starship_test_things.rs
```

### Specifying a C++ TestSuite class

In C++, a chosen TestSuite, which subclasses `testing::Test`, can be specified
with the `TEST_F()` macro. For example `TEST_F(SomeSubclassOfTestingTest,
Gadgets)`. The same can be done in Rust, albeit with a slight bit more
indirection. The `#[gtest_suite]` macro can be specified on the test function,
after the `#[gtest]` macro, in order to chose the TestSuite class. The macro
takes an argument which is the name of a C++ function that returns the output of
`rust_gtest_interop::rust_gtest_factory_for_subclass<T>()` where `T` is the
class to use as the TestSuite. For example:

In a C++ file:
```cpp
class ChosenClass: public testing::Test {};

/// This function can be used in #[gtest_suite].
extern "C" testing::Test* chosen_class_gtest_factory(void(*f)()) {
  return rust_gtest_interop::rust_gtest_factory_for_subclass<ChosenClass>(f);
}
```

In Rust tests:
```rs
use rust_gtest_interop::*;

#[gtest(ChosenClassTest, Gadgets)]
#[gtest_suite(chosen_class_gtest_factory)]
fn test() {
  // This test uses ChosenClass as its TestSuite.
}
```

Then the `ChosenClassTest.Gadgets` test will run with `ChosenClass` as its
TestSuite class. Note that the C++ function must be marked `extern "C"` at this
time, until we can generate access to C++-mangled functions from Rust.
