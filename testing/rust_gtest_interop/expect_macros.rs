// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(danakj): Reuse code for comparison macros with an expect_op!() macro?

/// Evaluates the given expression. If false, a failure is reported to Gtest.
///
/// # Examples
/// ```
/// expect_true(check_the_status_is_true());
/// ```
#[macro_export]
macro_rules! expect_true {
    ($e:expr) => {
        match &$e {
            val => {
                if !(*val) {
                    $crate::__private::add_failure_at(
                        file!(),
                        line!(),
                        &format!(
                            "Expected: {} is true\nActual: {} is {:?}",
                            stringify!($e),
                            stringify!($e),
                            *val,
                        ),
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
    ($e:expr) => {
        match &$e {
            val => {
                if *val {
                    $crate::__private::add_failure_at(
                        file!(),
                        line!(),
                        &format!(
                            "Expected: {} is false\nActual: {} is {:?}",
                            stringify!($e),
                            stringify!($e),
                            *val,
                        ),
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If not equal, a failure is reported to Gtest.
/// The expressions must evaluate to the same type, which must be PartialEq.
///
/// # Examples
/// ```
/// expect_eq(1 + 1, 2);
/// ```
#[macro_export]
macro_rules! expect_eq {
    ($e1:expr, $e2:expr) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 == *val2) {
                    $crate::__private::add_failure_at(
                        file!(),
                        line!(),
                        &format!(
                            "Expected: {} == {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2,
                        ),
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If equal, a failure is reported to Gtest.
/// The expressions must evaluate to the same type, which must be PartialEq.
///
/// # Examples
/// ```
/// expect_ne(1 + 1, 3);
/// ```
#[macro_export]
macro_rules! expect_ne {
    ($e1:expr, $e2:expr) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 != *val2) {
                    $crate::__private::add_failure_at(
                        file!(),
                        line!(),
                        &format!(
                            "Expected: {} != {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2,
                        ),
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If the first is not greater than the second, a
/// failure is reported to Gtest. The expressions must evaluate to the same type, which must be
/// PartialOrd. If a PartialOrd comparison fails, the result is false.
///
/// # Examples
/// ```
/// expect_gt(1 + 1, 1);
/// ```
#[macro_export]
macro_rules! expect_gt {
    ($e1:expr, $e2:expr) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 > *val2) {
                    $crate::__private::add_failure_at(
                        file!(),
                        line!(),
                        &format!(
                            "Expected: {} > {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2,
                        ),
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If the first is not less than the second, a
/// failure is reported to Gtest. The expressions must evaluate to the same type, which must be
/// PartialOrd. If a PartialOrd comparison fails, the result is false.
///
/// # Examples
/// ```
/// expect_lt(1 + 1, 1 + 2);
/// ```
#[macro_export]
macro_rules! expect_lt {
    ($e1:expr, $e2:expr) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 < *val2) {
                    $crate::__private::add_failure_at(
                        file!(),
                        line!(),
                        &format!(
                            "Expected: {} < {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2,
                        ),
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If the first is not greater than or equal to
/// the second, a failure is reported to Gtest. The expressions must evaluate to the same type,
/// which must be PartialOrd. If a PartialOrd comparison fails, the result is false.
///
/// # Examples
/// ```
/// expect_ge(1 + 1, 2);
/// ```
#[macro_export]
macro_rules! expect_ge {
    ($e1:expr, $e2:expr) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 >= *val2) {
                    $crate::__private::add_failure_at(
                        file!(),
                        line!(),
                        &format!(
                            "Expected: {} >= {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2,
                        ),
                    )
                }
            }
        }
    };
}

/// Evaluates and compares the two given expressions. If the first is not less than or equal to the
/// second, a failure is reported to Gtest. The expressions must evaluate to the same type, /which
/// must be PartialOrd. If a PartialOrd comparison fails, the result is false.
///
/// # Examples
/// ```
/// expect_le(2, 1 + 1);
/// ```
#[macro_export]
macro_rules! expect_le {
    ($e1:expr, $e2:expr) => {
        match (&$e1, &$e2) {
            (val1, val2) => {
                if !(*val1 <= *val2) {
                    $crate::__private::add_failure_at(
                        file!(),
                        line!(),
                        &format!(
                            "Expected: {} <= {}\nActual: {:?} vs {:?}",
                            stringify!($e1),
                            stringify!($e2),
                            *val1,
                            *val2,
                        ),
                    )
                }
            }
        }
    };
}

// TODO(danakj): There's a bunch more useful macros to write, even ignoring gmock:
// - EXPECT_NONFATAL_FAILURE
// - SCOPED_TRACE
// - EXPECT_DEATH
// - FAIL (fatal failure, would this work?)
// - ADD_FAILURE
// - SUCCEED
// - EXPECT_PANIC (catch_unwind() with an error if no panic, but this depends on us using a stdlib
//   with -Cpanic=unwind in tests, currently we use -Cpanic=abort.)
// - EXPECT_NO_PANIC (Like above, depende on -Cpanic=unwind, or else all panics just abort the
//   process.)
// - EXPECT_FLOAT_EQ (Comparison for equality within a small range.)
// - EXPECT_NEAR (Comparison for equality within a user-specified range.)

// TODO(danakj): Also consider adding an optional parameter that takes a message string to append to
// the expect macro's built-in message. We could also consider a format string + varargs like
// format!(), to save folks from writing format!() in that spot, if it would be common.
