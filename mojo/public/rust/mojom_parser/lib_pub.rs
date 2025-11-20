// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! ALL the parsing!
//
// FOR_RELEASE: Figure out the organization of this crate, what needs to be
// public, etc. For now, just (re-)export everything blindly.

chromium::import! {
    "//mojo/public/rust/mojom_parser:mojom_parser_core";
    "//mojo/public/rust/mojom_parser:parsing_attribute";
}

pub use mojom_parser_core::{deserialize, serialize};
pub use mojom_parser_core::{MojomParse, PrimitiveEnum};
pub use parsing_attribute::{MojomParse, PrimitiveEnum};
