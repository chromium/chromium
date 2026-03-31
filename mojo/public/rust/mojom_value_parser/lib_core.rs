// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This is the root of the `mojom_value_parser_core` crate. It has all the
//! actual parsing and deparsing logic.
//!
//! Since Chromium's testing model puts tests in a different crate, we just
//! blindly re-export everything so we can use it elsewhere. Users should not
//! depend on this crate; instead, they should use the `mojom_value_parser`
//! crate (rooted in `lib_pub.rs`), which only exports the `api` module and some
//! fundamental types.

mod api;
mod ast;
mod deparse_values;
mod errors;
mod message_header;
mod pack;
mod parse_primitives;
mod parse_values;
mod parsing_trait;

pub use crate::api::*;
pub use crate::ast::*;
pub use crate::deparse_values::*;
pub use crate::errors::*;
pub use crate::message_header::*;
pub use crate::pack::*;
pub use crate::parse_primitives::ParserData;
pub use crate::parse_values::*;
pub use crate::parsing_trait::*;
