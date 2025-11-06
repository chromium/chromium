// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests parsing and deparsing of values.
//!
//! FOR_RELEASE: Fill this out.
//!
//! Note that this only focuses on testing the parsing/deparsing stage. We rely
//! on test_mojomparse.rs to verify that the translations to/from rust types
//! are accurate.
//!
//! The types in this file correspond to those defined in
//! //mojo/public/rust/test_mojom/parser_unittests.mojom

chromium::import! {
    "//mojo/public/rust/mojom_parser:validation_parser";
}

use rust_gtest_interop::prelude::*;

// Quick test to make sure things basically work.
// FOR_RELEASE: Write real tests.
#[gtest(MojomParserTestSuite, TestSomething)]
fn test_something() -> Result<(), String> {
    // Should produce an array of 4-byte value 2, then 2-byte value 257
    let parsed_data = validation_parser::parse("[u4]2\n[u2]257")?;
    expect_eq!(parsed_data.data.as_ref(), &[2, 0, 0, 0, 1, 1]);
    expect_eq!(parsed_data.num_handles, 0);
    Ok(())
}
