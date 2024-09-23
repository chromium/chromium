// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//testing/rust_gtest_interop:gtest_attribute";
}

use std::pin::Pin;

/// Use `prelude:::*` to get access to all macros defined in this crate.
pub mod prelude {
    // The #[extern_test_suite("cplusplus::Type") macro.
    pub use gtest_attribute::extern_test_suite;
    // The #[gtest(TestSuite, TestName)] macro.
    pub use gtest_attribute::gtest;
    // Gtest expectation macros, which should be used to verify test expectations.
    // These replace the standard practice of using assert/panic in Rust tests
    // which would crash the test binary.
    pub use crate::expect_eq;
    pub use crate::expect_false;
    pub use crate::expect_ge;
    pub use crate::expect_gt;
    pub use crate::expect_le;
    pub use crate::expect_lt;
    pub use crate::expect_ne;
    pub use crate::expect_true;
}

// The gtest_attribute proc-macro crate makes use of small_ctor, with a path
// through this crate here to ensure it's available.
#[doc(hidden)]
pub extern crate small_ctor;

/// A marker trait that promises the Rust type is an FFI wrapper around a C++
/// class which subclasses `testing::Test`. In particular, casting a
/// `testing::Test` pointer to the implementing class type is promised to be
/// valid.
///
/// Implement this trait with the `#[extern_test_suite]` macro:
/// ```rs
/// #[extern_test_suite("cpp::type::wrapped::by::Foo")
/// unsafe impl TestSuite for Foo {}
/// ```
pub unsafe trait TestSuite {
    /// Gives the Gtest factory function on the C++ side which constructs the
    /// C++ class for which the implementing Rust type is an FFI wrapper.
    #[doc(hidden)]
    fn gtest_factory_fn_ptr() -> GtestFactoryFunction;
}

/// Matches the C++ type `rust_gtest_interop::GtestFactoryFunction`, with the
/// `testing::Test` type erased to `OpaqueTestingTest`.
///
/// We replace `testing::Test*` with `OpaqueTestingTest` because but we don't
/// know that C++ type in Rust, as we don't have a Rust generator giving access
/// to that type.
#[doc(hidden)]
pub type GtestFactoryFunction = unsafe extern "C" fn(
    f: extern "C" fn(Pin<&mut OpaqueTestingTest>),
) -> Pin<&'static mut OpaqueTestingTest>;

/// Opaque replacement of a C++ `testing::Test` type, which can only be used as
/// a pointer, since its size is incorrect. Only appears in the
/// GtestFactoryFunction signature, which is a function pointer that passed to
/// C++, and never run from within Rust.
///
/// See https://doc.rust-lang.org/nomicon/ffi.html#representing-opaque-structs
///
/// TODO(danakj): If there was a way, without making references to it into wide
/// pointers, we should make this type be !Sized.
#[repr(C)]
#[doc(hidden)]
pub struct OpaqueTestingTest {
    data: [u8; 0],
    marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
}

#[doc(hidden)]
pub trait TestResult {
    fn into_error_message(self) -> Option<String>;
}
impl TestResult for () {
    fn into_error_message(self) -> Option<String> {
        None
    }
}
// This impl requires an `Error` not just a `String` so that in the future we
// could print things like the backtrace too (though that field is currently
// unstable).
impl<E: Into<Box<dyn std::error::Error>>> TestResult for std::result::Result<(), E> {
    fn into_error_message(self) -> Option<String> {
        match self {
            Ok(_) => None,
            Err(e) => Some(format!("Test returned error: {}", e.into())),
        }
    }
}

// Internals used by code generated from the gtest-attriute proc-macro. Should
// not be used by human-written code.
#[doc(hidden)]
pub mod __private {
    use super::{GtestFactoryFunction, OpaqueTestingTest, Pin};

    /// Rust wrapper around C++'s rust_gtest_add_failure().
    ///
    /// The wrapper converts the file name into a C++-friendly string,
    /// and the line number into a C++-friendly signed int.
    ///
    /// TODO(crbug.com/40215436): We should be able to receive a C++-friendly
    /// file path.
    ///
    /// TODO(danakj): We should be able to pass a `c_int` directly to C++:
    /// https://github.com/dtolnay/cxx/issues/1015.
    pub fn add_failure_at(file: &'static str, line: u32, message: &str) {
        let null_term_file = std::ffi::CString::new(make_canonical_file_path(file)).unwrap();
        let null_term_message = std::ffi::CString::new(message).unwrap();

        extern "C" {
            fn rust_gtest_add_failure_at(
                file: *const std::ffi::c_char,
                line: i32,
                message: *const std::ffi::c_char,
            );

        }
        unsafe {
            rust_gtest_add_failure_at(
                null_term_file.as_ptr(),
                line.try_into().unwrap_or(-1),
                null_term_message.as_ptr(),
            )
        }
    }

    /// Turn a file!() string for a source file into a path from the root of the
    /// source tree.
    pub fn make_canonical_file_path(file: &str) -> String {
        // The path of the file here is relative to and prefixed with the crate root's
        // source file with the current directory being the build's output
        // directory. So for a generated crate root at gen/foo/, the file path
        // would look like `gen/foo/../../../../real/path.rs`. The last two `../
        // ` move up from the build output directory to the source tree root. As such,
        // we need to strip pairs of `something/../` until there are none left, and
        // remove the remaining `../` path components up to the source tree
        // root.
        //
        // Note that std::fs::canonicalize() does not work here since it requires the
        // file to exist, but we're working with a relative path that is rooted
        // in the build directory, not the current directory. We could try to
        // get the path to the build directory.. but this is simple enough.
        let (keep_rev, _) = std::path::Path::new(file).iter().rev().fold(
            (Vec::new(), 0),
            // Build the set of path components we want to keep, which we do by keeping a count of
            // the `..` components and then dropping stuff that comes before them.
            |(mut keep, dotdot_count), path_component| {
                if path_component == ".." {
                    // The `..` component will skip the next downward component.
                    (keep, dotdot_count + 1)
                } else if dotdot_count > 0 {
                    // Skip the component as we drop it with `..` later in the path.
                    (keep, dotdot_count - 1)
                } else {
                    // Keep this component.
                    keep.push(path_component);
                    (keep, dotdot_count)
                }
            },
        );
        // Reverse the path components, join them together, and write them into a
        // string.
        keep_rev
            .into_iter()
            .rev()
            .fold(std::path::PathBuf::new(), |path, path_component| path.join(path_component))
            .to_string_lossy()
            .to_string()
    }

    extern "C" {
        /// extern for C++'s rust_gtest_default_factory().
        /// TODO(danakj): We do this by hand because cxx doesn't support passing
        /// raw function pointers: https://github.com/dtolnay/cxx/issues/1011.
        pub fn rust_gtest_default_factory(
            f: extern "C" fn(Pin<&mut OpaqueTestingTest>),
        ) -> Pin<&'static mut OpaqueTestingTest>;
    }

    extern "C" {
        /// extern for C++'s rust_gtest_add_test().
        ///
        /// Note that the `factory` parameter is actually a C++ function
        /// pointer. TODO(danakj): We do this by hand because cxx
        /// doesn't support passing raw function pointers nor passing `*const c_char`: https://github.com/dtolnay/cxx/issues/1011 and
        /// https://github.com/dtolnay/cxx/issues/1015.
        pub fn rust_gtest_add_test(
            factory: GtestFactoryFunction,
            run_test_fn: extern "C" fn(Pin<&mut OpaqueTestingTest>),
            test_suite_name: *const std::os::raw::c_char,
            test_name: *const std::os::raw::c_char,
            file: *const std::os::raw::c_char,
            line: i32,
        );
    }

    /// Information used to register a function pointer as a test with the C++
    /// Gtest framework.
    pub struct TestRegistration {
        pub func: extern "C" fn(suite: Pin<&mut OpaqueTestingTest>),
        // TODO(danakj): These a C-String-Literals. Maybe we should expose that as a type
        // somewhere.
        pub test_suite_name: &'static [std::os::raw::c_char],
        pub test_name: &'static [std::os::raw::c_char],
        pub file: &'static [std::os::raw::c_char],
        pub line: u32,
        pub factory: GtestFactoryFunction,
    }

    /// Register a given test function with the C++ Gtest framework.
    ///
    /// This function is called from static initializers. It may only be called
    /// from the main thread, before main() is run. It may not panic, or
    /// call anything that may panic.
    pub fn register_test(r: TestRegistration) {
        let line = r.line.try_into().unwrap_or(-1);
        // SAFETY: The `factory` parameter to rust_gtest_add_test() must be a C++
        // function that returns a `testing::Test*` disguised as a
        // `OpaqueTestingTest`. The #[gtest] macro will use
        // `rust_gtest_interop::rust_gtest_default_factory()` by default.
        unsafe {
            rust_gtest_add_test(
                r.factory,
                r.func,
                r.test_suite_name.as_ptr(),
                r.test_name.as_ptr(),
                r.file.as_ptr(),
                line,
            )
        };
    }
}

mod expect_macros;
