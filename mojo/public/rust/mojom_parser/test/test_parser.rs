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
    "//mojo/public/rust/mojom_parser:mojom_parser_core";
    "//mojo/public/rust/mojom_parser:validation_parser";
}

use mojom_parser_core::*;
use rust_gtest_interop::prelude::*;

// Verify that `value` serializes to the binary data represented by `data`,
// which is a string in the mojom validation text format, as defined in
// //mojo/public/cpp/bindings/tests/validation_test_input_parser.h
fn validate_parsing<T>(value: T, data: &str) -> anyhow::Result<()>
where
    T: MojomParse + PartialEq + std::fmt::Debug + Clone,
{
    // Helper function so we can use the question mark operator, but also
    // append context to it regardless of where we return.
    let validate_parsing_internal = || -> anyhow::Result<()> {
        let wire_data = validation_parser::parse(data)
            .map_err(anyhow::Error::msg)?
            // We currently don't do anything with handles, so only look at the data field
            .data;
        expect_eq!(
            value,
            parse_single_value_for_testing(wire_data.as_ref(), T::wire_type())?.try_into()?
        );
        // FOR_RELEASE: We shouldn't need to clone here
        expect_eq!(
            wire_data.as_ref(),
            deparse_single_value_for_testing(&value.clone().into(), T::wire_type())?
        );
        Ok(())
    };

    // We could also use anyhow's Context trait, but that overwrites the old context
    // since we can't control the printing format gtest uses.
    validate_parsing_internal()
        .map_err(|err| anyhow::anyhow!("{}\nRust value: {:?}\nWire Data: {}", err, value, data))
}

#[gtest(MojomParser, TestPrimitiveParsing)]
fn test_primitives() -> anyhow::Result<()> {
    validate_parsing(7u8, "[u1]7")?;
    validate_parsing(17u16, "[u2]17")?;
    validate_parsing(1234567u32, "[u4]1234567")?;
    validate_parsing(123456789101112u64, "[u8]123456789101112")?;
    // FOR_RELEASE: More of this, also make sure that we correctly fail to
    // parse them sometimes.
    Ok(())
}
