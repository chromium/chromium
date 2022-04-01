// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Use `prelude:::*` to get access to all macros defined in this crate.
pub mod prelude {
    // The #[gtest(TestSuite, TestName)] macro.
    pub use gtest_attribute::gtest;
    // Gtest expectation macros, which should be used to verify test expectations. These replace the
    // standard practice of using assert/panic in Rust tests which would crash the test binary.
    pub use crate::expect_eq;
    pub use crate::expect_false;
    pub use crate::expect_ge;
    pub use crate::expect_gt;
    pub use crate::expect_le;
    pub use crate::expect_lt;
    pub use crate::expect_ne;
    pub use crate::expect_true;
}

// The gtest_attribute proc-macro crate makes use of small_ctor, with a path through this crate here
// to ensure it's available.
#[doc(hidden)]
pub extern crate small_ctor;

pub trait TestResult {
    fn into_error_message(self) -> Option<String>;
}
impl TestResult for () {
    fn into_error_message(self) -> Option<String> {
        None
    }
}
// This impl requires an `Error` not just a `String` so that in the future we could print things
// like the backtrace too (though that field is currently unstable).
impl<E: Into<Box<dyn std::error::Error>>> TestResult for std::result::Result<(), E> {
    fn into_error_message(self) -> Option<String> {
        match self {
            Ok(_) => None,
            Err(e) => Some(format!("Test returned error: {}", e.into())),
        }
    }
}

/// Matches the C++ type `rust_gtest_interop::GtestFactoryFunction`, except replaces the return
/// type with an opaque `GtestSuitePtr`
///
/// The actual return type is `testing::Test*` but we don't know that type in Rust currently, as we
/// don't have a Rust generator generating access to that type.
pub type GtestFactoryFunction = unsafe extern "C" fn(f: extern "C" fn()) -> GtestSuitePtr;

/// Opaque replacement of a C++ `testing::Test*` pointer type. Only appears in the
/// GtestFactoryFunction signature, which is a function pointer that passed to C++, and never run
/// from within Rust.
#[repr(C)]
pub struct GtestSuitePtr(usize);

// Internals used by code generated from the gtest-attriute proc-macro. Should not be used by
// human-written code.
#[doc(hidden)]
pub mod __private {
    use super::GtestFactoryFunction;

    #[cxx::bridge(namespace=rust_gtest_interop)]
    mod ffi {
        unsafe extern "C++" {
            include!("testing/rust_gtest_interop/rust_gtest_interop.h");
            // TODO(danakj): C++ wants an int, but cxx doesn't support c_int, so we use i32.
            // Similarly, C++ wants a char* but cxx doesn't support c_char, so we use u8.
            // https://github.com/dtolnay/cxx/issues/1015
            unsafe fn rust_gtest_add_failure_at(file: *const u8, line: i32, message: &str);
        }
    }

    /// Rust wrapper around the same C++ method.
    ///
    /// We have a wrapper to convert the file name into a C++-friendly string, and the line number
    /// into a C++-friendly signed int.
    ///
    /// TODO(crbug.com/1298175): We should be able to receive a C++-friendly file path.
    ///
    /// TODO(danakj): We should be able to pass a `c_int` directly to C++:
    /// https://github.com/dtolnay/cxx/issues/1015.
    pub fn add_failure_at(file: &'static str, line: u32, message: &str) {
        // TODO(danakj): Our own file!() macro should strip "../../" from the front of the string.
        let file = file.replace("../", "");
        // TODO(danakj): Write a file!() macro that null-terminates the string so we can use it here
        // directly and also for constructing base::Location. Then.. pass a base::Location here?
        let null_term_file = std::ffi::CString::new(file).unwrap();
        unsafe {
            ffi::rust_gtest_add_failure_at(
                null_term_file.as_ptr() as *const u8,
                line.try_into().unwrap_or(-1),
                message,
            )
        }
    }

    /// Wrapper that calls C++ rust_gtest_add_test().
    ///
    /// Note that the `factory` parameter is actually a C++ function pointer, of type
    /// rust_gtest_interop::GtestFactoryFunction.
    ///
    /// # Safety
    ///
    /// The `factory` function pointer is to a C++ function that returns a `testing::Test*`
    /// disguised as a `GtestSuitePtr` since we don't have generated bindings for the
    /// `testing::Test` class.
    ///
    /// TODO(danakj): We do this by hand because cxx doesn't support passing raw function pointers
    /// nor passing `*const c_char`: https://github.com/dtolnay/cxx/issues/1011 and
    /// https://github.com/dtolnay/cxx/issues/1015.
    unsafe fn rust_gtest_add_test(
        factory: GtestFactoryFunction,
        func: extern "C" fn(),
        test_suite_name: *const std::os::raw::c_char,
        test_name: *const std::os::raw::c_char,
        file: *const std::os::raw::c_char,
        line: i32,
    ) {
        extern "C" {
            /// The C++ mangled name for rust_gtest_interop::rust_gtest_add_test(). This comes from
            /// `objdump -t` on the C++ object file.
            fn _ZN18rust_gtest_interop19rust_gtest_add_testEPFPN7testing4TestEPFvvEES4_PKcS8_S8_i(
                factory: GtestFactoryFunction,
                func: extern "C" fn(),
                test_suite_name: *const std::os::raw::c_char,
                test_name: *const std::os::raw::c_char,
                file: *const std::os::raw::c_char,
                line: i32,
            );
        }

        _ZN18rust_gtest_interop19rust_gtest_add_testEPFPN7testing4TestEPFvvEES4_PKcS8_S8_i(
            factory,
            func,
            test_suite_name,
            test_name,
            file,
            line,
        )
    }

    /// Information used to register a function pointer as a test with the C++ Gtest framework.
    pub struct TestRegistration {
        pub func: extern "C" fn(),
        // TODO(danakj): These a C-String-Literals. Maybe we should expose that as a type somewhere.
        pub test_suite_name: &'static [std::os::raw::c_char],
        pub test_name: &'static [std::os::raw::c_char],
        pub file: &'static [std::os::raw::c_char],
        pub line: u32,
        pub factory: GtestFactoryFunction,
    }

    /// Register a given test function with the C++ Gtest framework.
    ///
    /// This function is called from static initializers. It may only be called from the main
    /// thread, before main() is run. It may not panic, or call anything that may panic.
    pub fn register_test(r: TestRegistration) {
        let line = r.line.try_into().unwrap_or(-1);
        // SAFETY: The `factory` parameter to rust_gtest_add_test() must be a C++ function that
        // returns a `testing::Test*` disguised as a `GTestSuitePtr`. The #[gtest] macro will use
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
