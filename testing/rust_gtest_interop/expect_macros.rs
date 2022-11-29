// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(danakj): Reuse code for comparison macros with an expect_op!() macro?

/// Internal helper to log an expectation failure. Other expect_* macros invoke
/// it with their standard expectation message, plus optionally a user-provided
/// custom string.
///
/// Both the the expectation message and the user-provided message are format
/// strings with arguments. To disambiguate between them, the expectation
/// message is wrapped in extra parentheses.
#[macro_export]
macro_rules! internal_add_expectation_failure {
    // Rule that both the below are forwarded to.
    (@imp $fmt:literal, $($arg:tt)+) => {
        $crate::__private::add_failure_at(
            file!(),
            line!(),
            &format!($fmt, $($arg)+),
        )
    };

    // Add a failure with the standard message.
    (($expectation:literal, $($e:tt)+)) => {
        $crate::internal_add_expectation_failure!(@imp $expectation, $($e)+)
    };

    // Add a failure with the standard message plus an additional message.
    (($expectation:literal, $($e:tt)+), $($arg:tt)+) => {
        $crate::internal_add_expectation_failure!(@imp
            "{}\n\n{}",
            format_args!($expectation, $($e)+),
            format_args!($($arg)+),
        )
    };
}

/// Evaluates the given expression. If false, a failure is reported to Gtest.
///
/// # Examples
/// ```
/// expect_true(check_the_status_is_true());
/// ```
#[macro_export]
macro_rules! expect_true {
    ($e:expr $(, $($arg:tt)*)?) => {
        match &$e {
            val => {
                if !(*val) {
                    $crate::internal_add_expectation_failure!(
                        (
                            "Expected: {} is true\nActual: {} is {:?}",
                            stringify!($e),
                            stringify!($e),
                            *val
                        )
                        $(, $($arg)*)?
                    )
                }
            }
        }
    };
}

/// Evaluates the given expression. If true, a failure is reported to Gtest.
///
/// # Examples
/// ```
/// expect_false(check_the_status_is_false());
/// ```
#[macro_export]
macro_rules! expect_false {
    ($e:expr $(, $($arg:tt)*)?) => {
        match &$e {
            val => {
                if *val {
                    $crate::internal_add_expectation_failure!(
                        (
                            "Expected: {} is false\nActual: {} is {:?}",
                            stringify!($e),
                            stringify!($e),
                            *val
                        )
                        $(, $($arg)*)?
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If not equal, a failure is
/// reported to Gtest. The expressions must evaluate to the same type, which
/// must be PartialEq.
///
/// # Examples
/// ```
/// expect_eq(1 + 1, 2);
/// ```
#[macro_export]
macro_rules! expect_eq {
    ($e1:expr, $e2:expr $(, $($arg:tt)*)?) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 == *val2) {
                    $crate::internal_add_expectation_failure!(
                        (
                            "Expected: {} == {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2
                        )
                        $(, $($arg)*)?
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If equal, a failure is
/// reported to Gtest. The expressions must evaluate to the same type, which
/// must be PartialEq.
///
/// # Examples
/// ```
/// expect_ne(1 + 1, 3);
/// ```
#[macro_export]
macro_rules! expect_ne {
    ($e1:expr, $e2:expr $(, $($arg:tt)*)?) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 != *val2) {
                    $crate::internal_add_expectation_failure!(
                        (
                            "Expected: {} != {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2
                        )
                        $(, $($arg)*)?
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If the first is not
/// greater than the second, a failure is reported to Gtest. The expressions
/// must evaluate to the same type, which must be PartialOrd. If a PartialOrd
/// comparison fails, the result is false.
///
/// # Examples
/// ```
/// expect_gt(1 + 1, 1);
/// ```
#[macro_export]
macro_rules! expect_gt {
    ($e1:expr, $e2:expr $(, $($arg:tt)*)?) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 > *val2) {
                    $crate::internal_add_expectation_failure!(
                        (
                            "Expected: {} > {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2
                        )
                        $(, $($arg)*)?
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If the first is not less
/// than the second, a failure is reported to Gtest. The expressions must
/// evaluate to the same type, which must be PartialOrd. If a PartialOrd
/// comparison fails, the result is false.
///
/// # Examples
/// ```
/// expect_lt(1 + 1, 1 + 2);
/// ```
#[macro_export]
macro_rules! expect_lt {
    ($e1:expr, $e2:expr $(, $($arg:tt)*)?) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 < *val2) {
                    $crate::internal_add_expectation_failure!(
                        (
                            "Expected: {} < {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2
                        )
                        $(, $($arg)*)?
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If the first is not
/// greater than or equal to the second, a failure is reported to Gtest. The
/// expressions must evaluate to the same type, which must be PartialOrd. If a
/// PartialOrd comparison fails, the result is false.
///
/// # Examples
/// ```
/// expect_ge(1 + 1, 2);
/// ```
#[macro_export]
macro_rules! expect_ge {
    ($e1:expr, $e2:expr $(, $($arg:tt)*)?) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 >= *val2) {
                    $crate::internal_add_expectation_failure!(
                        (
                            "Expected: {} >= {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2
                        ) $(, $($arg)*)?
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If the first is not less
/// than or equal to the second, a failure is reported to Gtest. The expressions
/// must evaluate to the same type, /which must be PartialOrd. If a PartialOrd
/// comparison fails, the result is false.
///
/// # Examples
/// ```
/// expect_le(2, 1 + 1);
/// ```
#[macro_export]
macro_rules! expect_le {
    ($e1:expr, $e2:expr $(, $($arg:tt)*)?) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 <= *val2) {
                    $crate::internal_add_expectation_failure!(
                        (
                            "Expected: {} <= {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2
                        ) $(, $($arg)*)?
                    )
                }
            }
        }
    };
}

// TODO(danakj): There's a bunch more useful macros to write, even ignoring
// gmock:
// - EXPECT_NONFATAL_FAILURE
// - SCOPED_TRACE
// - EXPECT_DEATH
// - FAIL (fatal failure, would this work?)
// - ADD_FAILURE
// - SUCCEED
// - EXPECT_PANIC (catch_unwind() with an error if no panic, but this depends on
//   us using a stdlib with -Cpanic=unwind in tests, currently we use
//   -Cpanic=abort.)
// - EXPECT_NO_PANIC (Like above, depende on -Cpanic=unwind, or else all panics
//   just abort the process.)
// - EXPECT_FLOAT_EQ (Comparison for equality within a small range.)
// - EXPECT_NEAR (Comparison for equality within a user-specified range.)
