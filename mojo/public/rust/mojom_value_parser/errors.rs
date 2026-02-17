// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Errors that the parser might return. User-visible.
//! FOR_RELEASE: Docs

pub type ParsingResult<T> = Result<T, ParsingError>;
impl std::error::Error for ParsingError {}

#[derive(Debug)]
pub struct ParsingError {
    /// The number of bytes from the beginning of the message at which the error
    /// occurred.
    pub offset: usize,
    pub ty: ParsingErrorType,
}

#[derive(Debug)]
pub enum ParsingErrorType {
    /// Indicates that we ran out of data while parsing
    NotEnoughData {
        /// A description of what we tried to parse (a type, or padding, etc)
        tried_to_parse: String,
        expected_size: usize,
        /// How much data was actually left
        remaining_bytes: usize,
    },
    /// Indicates that there was data left over after we finished parsing
    TooMuchData { remaining_bytes: usize },
    /// Indicates a pointer that was either too small, too large to fit into a
    /// usize, unexpectedly null, or not divisible by 8 (so the pointed-to data
    /// wouldn't be aligned)
    InvalidPointer { value: u64 },
    /// Indicates that a nested field wasn't at the pointed-to location
    WrongPointer { name: String, expected_offset: usize, actual_offset: usize },
    /// Indicates a size (as encoded in a struct/array header) was either too
    /// small, too large to fit into a usize, or not divisible by 8.
    InvalidSize { value: u32 },
    /// Indicates that a struct or array had more bytes than its header claimed
    WrongSize { expected_size: usize, actual_size: usize },
    /// Indicates that the message contained an invalid discriminant for a
    /// non-extensible enum or union type
    /// We don't carry the expected values because there isn't an easy way to
    /// show them to the user
    InvalidDiscriminant { value: u32 },
    /// Indicates that a sized array had an incorrect number of elements
    WrongArraySize { expected: usize, actual: usize },
    /// Indicates that the bytes in a string weren't UTF-8 encoded
    NonUTF8String { err: std::string::FromUtf8Error },
    /// Indicates that a map had a duplicate key
    DuplicateMapKey { dup: crate::ast::MojomValue },
    /// Indicates that the key and value arrays for a map were different lengths
    MismatchedMap { key_len: usize, value_len: usize },
    /// Indicates that the corresponding mojom feature has yet to be implemented
    NotImplemented { feature_name: String },
    /// Indicates that we failed to retrieve a handle from the given index
    InvalidHandleIndex { idx: usize },
}

impl ParsingError {
    pub fn not_enough_data(
        offset: usize,
        tried_to_parse: String,
        expected_size: usize,
        remaining_bytes: usize,
    ) -> ParsingError {
        ParsingError {
            offset,
            ty: ParsingErrorType::NotEnoughData { tried_to_parse, expected_size, remaining_bytes },
        }
    }

    pub fn too_much_data(offset: usize, remaining_bytes: usize) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::TooMuchData { remaining_bytes } }
    }

    pub fn invalid_pointer(offset: usize, value: u64) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::InvalidPointer { value } }
    }

    pub fn invalid_size(offset: usize, value: u32) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::InvalidSize { value } }
    }

    pub fn wrong_pointer(
        offset: usize,
        name: String,
        expected_offset: usize,
        actual_offset: usize,
    ) -> ParsingError {
        ParsingError {
            offset,
            ty: ParsingErrorType::WrongPointer { name, expected_offset, actual_offset },
        }
    }

    pub fn wrong_size(offset: usize, expected_size: usize, actual_size: usize) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::WrongSize { expected_size, actual_size } }
    }

    pub fn invalid_discriminant(offset: usize, value: u32) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::InvalidDiscriminant { value } }
    }

    pub fn wrong_array_size(offset: usize, expected: usize, actual: usize) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::WrongArraySize { expected, actual } }
    }

    pub fn non_utf8_string(offset: usize, err: std::string::FromUtf8Error) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::NonUTF8String { err } }
    }

    pub fn duplicate_map_key(offset: usize, dup: crate::ast::MojomValue) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::DuplicateMapKey { dup } }
    }

    pub fn mismatched_map(offset: usize, key_len: usize, value_len: usize) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::MismatchedMap { key_len, value_len } }
    }

    pub fn not_implemented(offset: usize, feature_name: String) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::NotImplemented { feature_name } }
    }

    pub fn invalid_handle_index(offset: usize, idx: usize) -> ParsingError {
        ParsingError { offset, ty: ParsingErrorType::InvalidHandleIndex { idx } }
    }
}

// These messages necessarily refer to details of the binary format, and so they
// are mostly suitable for debugging, not for end-users (who don't really care
// why a message was invalid, since it should never happen unless a parser/
// deparser is misbehaving).
impl std::fmt::Display for ParsingError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        writeln!(
            f,
            "An error occurred at byte {} when parsing the message. Details:",
            self.offset
        )?;
        match &self.ty {
            ParsingErrorType::NotEnoughData { tried_to_parse, expected_size, remaining_bytes } => {
                write!(
                    f,
                    "Tried to parse {tried_to_parse} ({expected_size} bytes), \
                     but only {remaining_bytes} bytes remained."
                )
            }
            ParsingErrorType::TooMuchData { remaining_bytes } => {
                write!(f, "There were {remaining_bytes} bytes remaining after parsing finished.")
            }
            ParsingErrorType::InvalidPointer { value } => {
                if *value < 8 {
                    write!(f, "Pointer value {value} is less than its own size (8).")
                } else if *value % 8 != 0 {
                    write!(
                        f,
                        "Pointer value {value} points to unaligned data \
                         (because it is not divisible by 8)."
                    )
                } else {
                    write!(f, "Pointer value {value} doesn't fit into 64 bits.")
                }
            }
            ParsingErrorType::InvalidSize { value } => {
                if *value < 8 {
                    write!(f, "Size value {value} is less than the size of a header(8).")
                } else if *value % 8 != 0 {
                    write!(f, "Size value {value} is not divisible by 8.")
                } else {
                    write!(f, "Pointer value {value} doesn't fit into 32 bits.")
                }
            }
            ParsingErrorType::WrongPointer { name, expected_offset, actual_offset } => {
                write!(
                    f,
                    "Expected to find nested field {name} at {expected_offset} bytes from the \
                     beginning of the struct, but it was actually at {actual_offset} bytes."
                )
            }
            ParsingErrorType::WrongSize { expected_size, actual_size } => {
                write!(
                    f,
                    "Struct/Array claimed to have {expected_size} bytes, \
                     but we parsed {actual_size} bytes."
                )
            }
            ParsingErrorType::InvalidDiscriminant { value } => {
                write!(f, "Enum/Union value {value} is not a valid discriminant for its type.")
            }
            ParsingErrorType::WrongArraySize { expected, actual } => write!(
                f,
                "Expected array to have {expected} elements, but it had {actual} elements."
            ),
            ParsingErrorType::NonUTF8String { err } => {
                write!(f, "A string in the mojom message wasn't UTF-8 encoded!\n{err}")
            }
            ParsingErrorType::DuplicateMapKey { dup } => {
                write!(f, "The following map key appeared more than once: {dup:?}")
            }
            ParsingErrorType::MismatchedMap { key_len, value_len } => {
                write!(f, "Map had {key_len} keys and {value_len} values.")
            }
            ParsingErrorType::NotImplemented { feature_name } => {
                write!(f, "The rust bindings do not yet support {feature_name}")
            }
            ParsingErrorType::InvalidHandleIndex { idx } => {
                write!(
                    f,
                    "Failed to retrieve the handle attached to the message at index {idx}. \
                    Either the index was invalid or it was already retrieved."
                )
            }
        }
    }
}
