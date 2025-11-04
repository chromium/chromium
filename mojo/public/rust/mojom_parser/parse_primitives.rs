// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Defines the primitive parsers that the rest of the mojom parsers build upon.
//!
//! This module contains the lowest-level parsers. These parse simple, generic
//! data types (ints and floats), and interact directly with the encoded data as
//! a byte stream.
//!
//! In this library, a *parser* is a function that takes in a byte array (and
//! possibly some other information) and returns a `Result<value>`, for some
//! type `value`. In the process, it mutates the input data stream, dropping
//! however much data it read.
//!
//! Since Mojom messages encode expected offsets of data for validation
//! purposes, parsers also track how much data has been parsed in total.
//!
//! All parsers ensure that enough data exists, and return an error result if
//! not.

// FOR_RELEASE: This strategy basically re-invents nom, so we should consider
// switching to that if we intend to keep going down this route.

// FOR_RELEASE: The original doc says everything is little-endian, but someone
// told me it might all be host-endian. Figure that out before it causes
// problems.

use crate::errors::*;
use std::any::type_name;

/// The input to a parser
pub struct ParserData<'a> {
    remaining_bytes: &'a [u8],
    bytes_parsed: usize,
}

// Since the primitive parsers require mutable references, encapsulate the
// internal representation so we don't accidentally mutate it elsewhere.
impl<'a> ParserData<'a> {
    /// Create a new ParserData from a byte array.
    pub fn new(data: &'a [u8]) -> ParserData<'a> {
        ParserData { remaining_bytes: data, bytes_parsed: 0 }
    }

    /// How many bytes have been parsed since the ParserData was created.
    pub fn bytes_parsed(&self) -> usize {
        self.bytes_parsed
    }

    /// How many bytes remain to be parsed.
    pub fn remaining_bytes(&self) -> usize {
        self.remaining_bytes.len()
    }
}

/// Skips the next `bytes_to_parse` bytes, assuming they exist.
pub fn parse_padding(data: &mut ParserData, bytes_to_parse: usize) -> ParsingResult<()> {
    let mk_err = || ParsingError {
        offset: data.bytes_parsed(),
        ty: ParsingErrorType::NotEnoughData {
            tried_to_parse: format!("padding"),
            expected_size: bytes_to_parse,
            remaining_bytes: data.remaining_bytes.len(),
        },
    };
    data.remaining_bytes =
        data.remaining_bytes.get((bytes_to_parse as usize)..).ok_or_else(mk_err)?;
    data.bytes_parsed += bytes_to_parse;
    Ok(())
}

// Declares a function named $name, which takes a byte slice (&[u8]), reads the
// first $size_in_bytes entries, and interprets them as a value of type
// $target_type, assuming they are in little-endian order.
// Returns the parsed value, and the number of bytes parsed, and
// the remainder of the byte slice.
// Returns an error if there aren't enough bytes in the slice.
macro_rules! declare_primitive_parser {
    ($target_type:ty, $size_in_bytes:literal, $name:ident) => {
        pub fn $name(data: &mut ParserData) -> ParsingResult<$target_type> {
            let mk_err = || ParsingError {
                offset: data.bytes_parsed(),
                ty: ParsingErrorType::NotEnoughData {
                    tried_to_parse: type_name::<$target_type>().to_string(),
                    expected_size: $size_in_bytes,
                    remaining_bytes: data.remaining_bytes.len(),
                },
            };
            let (head, tail) =
                data.remaining_bytes.split_first_chunk::<$size_in_bytes>().ok_or_else(mk_err)?;
            let ret = <$target_type>::from_le_bytes(*head);
            data.remaining_bytes = tail;
            data.bytes_parsed += $size_in_bytes;
            Ok(ret)
        }
    };
}

declare_primitive_parser!(u8, 1, parse_u8);
declare_primitive_parser!(u16, 2, parse_u16);
declare_primitive_parser!(u32, 4, parse_u32);
declare_primitive_parser!(u64, 8, parse_u64);
declare_primitive_parser!(i8, 1, parse_i8);
declare_primitive_parser!(i16, 2, parse_i16);
declare_primitive_parser!(i32, 4, parse_i32);
declare_primitive_parser!(i64, 8, parse_i64);
declare_primitive_parser!(f32, 4, parse_f32);
declare_primitive_parser!(f64, 8, parse_f64);
