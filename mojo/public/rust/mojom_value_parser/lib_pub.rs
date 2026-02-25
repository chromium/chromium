// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! ALL the parsing!
//
// FOR_RELEASE: Figure out the organization of this crate, what needs to be
// public, etc. For now, just (re-)export everything blindly.

chromium::import! {
    "//mojo/public/rust/mojom_value_parser:mojom_value_parser_core";
    "//mojo/public/rust/mojom_value_parser:parsing_attribute";
}

pub use mojom_value_parser_core::{deserialize, deserialize_exact, serialize, ParsingResult};
pub use mojom_value_parser_core::{
    MessageHeader, MessageHeaderV1, MessageHeaderV2, MessageHeaderV3,
};
pub use mojom_value_parser_core::{MojomParse, PrimitiveEnum};
pub use parsing_attribute::{MojomParse, PrimitiveEnum};
