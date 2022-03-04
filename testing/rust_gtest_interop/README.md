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

To add a Rust file to the test suite, simply add it to the `rs_sources`, and to
the _crate root_. If this is the first Rust file being added, then you will have
to write the crate root as well.

`BUILD.gn`:
```gn
test("some_unittests") {
  sources = [
    "a_cpp_file.cc",
    "another_cpp_file.cc",
  ]
  rs_sources = [
    "some_unittests.rs",
    "a_rust_file.rs",
  ]
  crate_root = "some_unittests.rs"
  deps = [
    "//testing/gtest",
  ]
}
```

The crate root is named after the unit test target. In this case, it is
`some_unittests.rs`. The file just lists the modules in the test suite. By
default the test files are expected to be in the crate root's directory, but you
can use the `#[path]` attribute to add tests in subdirectories too.

`some_unittests.rs`:
```rs
#[path = "a_rust_file.rs"]
mod a_rust_file;
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
