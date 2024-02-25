// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use std::path::PathBuf;

#[gtest(Test, InTopModule)]
fn test() {
    expect_true!(true);
}

#[gtest(Test, WithCustomMessage)]
fn test() {
    expect_true!(true, "foo");
    expect_true!(true, "foo {}", 1);
    expect_eq!(5, 5, "math stopped working");
    expect_eq!(5 + 5, 10, "uh {}", "oh");
}

mod module1 {
    use super::*;

    #[gtest(Test, InChildModule)]
    fn test() {
        expect_true!(true);
    }

    mod module2 {
        use super::*;

        #[gtest(Test, InGrandChildModule)]
        fn test() {
            expect_true!(true);
        }
    }
}

#[allow(dead_code)]
fn bar() {
    #[gtest(Test, InFunctionBody)]
    fn test() {
        expect_true!(true);
    }
}

mod module3 {
    use super::*;

    #[allow(dead_code)]
    fn bar() {
        #[gtest(Test, InFunctionBodyInChildModule)]
        fn test() {
            expect_true!(true);
        }
    }
}

#[gtest(ExactSuite, ExactTest)]
fn test() {}

#[gtest(Test, WithResultType)]
fn test() -> std::io::Result<()> {
    expect_true!(true);
    Ok(())
}

#[gtest(Test, WithBoxResultType)]
fn test() -> std::result::Result<(), Box<dyn std::error::Error>> {
    expect_true!(true);
    Ok(())
}

// This test intentionally fails due to returning Err, and displays the message
// "uhoh."
#[gtest(Test, DISABLED_WithError)]
fn test() -> std::result::Result<(), Box<dyn std::error::Error>> {
    expect_true!(true);
    Err("uhoh".into())
}

// TODO(danakj): It would be nice to test expect macros, but we would need to
// hook up EXPECT_NONFATAL_FAILURE to do so. There's no way to fail a test in a
// way that we accept, the bots see the failure even if the process returns 0.
// #[gtest(ExpectFailTest, Failures)]
// fn test() {
//     expect_eq!(1 + 1, 1 + 2);
//     expect_ne!(2 + 3, 3 + 2);
//     expect_lt!(1 + 1, 1 + 0);
//     expect_gt!(1 + 0, 1 + 1);
//     expect_le!(1 + 1, 1 + 0);
//     expect_ge!(1 + 0, 1 + 1);
//     expect_true!(true && false);
//     expect_false!(true || false);
//     unsafe { COUNTER += 1 };
// }

#[gtest(Test, Paths)]
fn test() {
    let expected_result =
        ["foo", "bar.rs"].iter().collect::<PathBuf>().to_string_lossy().to_string();

    expect_eq!(
        rust_gtest_interop::__private::make_canonical_file_path("foo/bar.rs"),
        expected_result
    );
    expect_eq!(
        rust_gtest_interop::__private::make_canonical_file_path("../foo/bar.rs"),
        expected_result
    );
    expect_eq!(
        rust_gtest_interop::__private::make_canonical_file_path("../../foo/bar.rs"),
        expected_result
    );
    expect_eq!(
        rust_gtest_interop::__private::make_canonical_file_path("a/../foo/bar.rs"),
        expected_result
    );
    expect_eq!(
        rust_gtest_interop::__private::make_canonical_file_path("a/../../../foo/bar.rs"),
        expected_result
    );
    expect_eq!(
        rust_gtest_interop::__private::make_canonical_file_path("a/../b/../../foo/bar.rs"),
        expected_result
    );

    #[cfg(windows)]
    {
        expect_eq!(
            rust_gtest_interop::__private::make_canonical_file_path(r"foo\bar.rs"),
            r"foo\bar.rs"
        );

        expect_eq!(
            rust_gtest_interop::__private::make_canonical_file_path(r"..\foo\bar.rs"),
            r"foo\bar.rs"
        );

        expect_eq!(
            rust_gtest_interop::__private::make_canonical_file_path(r"..\..\foo\bar.rs"),
            r"foo\bar.rs"
        );

        expect_eq!(
            rust_gtest_interop::__private::make_canonical_file_path(r"a\..\foo\bar.rs"),
            r"foo\bar.rs"
        );

        expect_eq!(
            rust_gtest_interop::__private::make_canonical_file_path(r"a\..\..\..\foo\bar.rs"),
            r"foo\bar.rs"
        );

        expect_eq!(
            rust_gtest_interop::__private::make_canonical_file_path(r"a\..\b\..\..\foo\bar.rs"),
            expected_result
        );
    }
}
