// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! The brotli decoder of //net when `features::kRustBrotliInNet` is enabled:
//! the `brotli-decompressor` crate, which C++ calls through the Crubit
//! bindings of this crate (`net/filter/rust_brotli_decoder.h`).
//!
//! C++ can't call the crate directly, because its decoder is generic over its
//! allocators. `Decoder` uses the crate's `StandardAlloc`, which allocates with
//! the Rust global allocator.

use brotli_decompressor::{
    BrotliDecompressStreamWithDictionary, BrotliResult, BrotliState, StandardAlloc,
};

/// What `Decoder::decompress_stream()` did.
#[derive(Clone, Copy, Default)]
pub struct DecodeResult {
    /// A `BrotliDecoderResult` of //third_party/brotli, whose values
    /// net/filter/brotli_source_stream.cc checks with `static_assert`s.
    pub result: i32,
    /// How many bytes of the input the decoder consumed.
    pub bytes_read: usize,
    /// How many bytes the decoder wrote to the output.
    pub bytes_written: usize,
}

/// A brotli decoder, like a `BrotliDecoderState` of //third_party/brotli. Like
/// `BrotliDecoderCreateInstance()`, it rejects the large-window extension of
/// brotli, which RFC 7932 doesn't define.
pub struct Decoder {
    state: BrotliState<StandardAlloc, StandardAlloc, StandardAlloc>,
}

impl Default for Decoder {
    fn default() -> Self {
        Self {
            state: BrotliState::new_strict(
                StandardAlloc::default(),
                StandardAlloc::default(),
                StandardAlloc::default(),
            ),
        }
    }
}

impl Decoder {
    /// Makes the decoder use a raw shared dictionary (RFC 9842) of `size`
    /// bytes, like `BrotliDecoderAttachDictionary()` with
    /// `BROTLI_SHARED_DICTIONARY_RAW`. The decoder doesn't keep a pointer to
    /// the dictionary: every `decompress_stream()` call lends it instead.
    /// Returns false if the decoder can't use the dictionary.
    pub fn attach_dictionary(&mut self, size: usize) -> bool {
        self.state.attach_external_dictionary(size)
    }

    /// Decodes as much of `input` into `output` as it can, like
    /// `BrotliDecoderDecompressStream()`. `dictionary` must be the dictionary
    /// of `attach_dictionary()`, or empty without one.
    pub fn decompress_stream(
        &mut self,
        dictionary: &[u8],
        input: &[u8],
        output: &mut [u8],
    ) -> DecodeResult {
        let mut available_in = input.len();
        let mut bytes_read = 0;
        let mut available_out = output.len();
        let mut bytes_written = 0;
        let mut total_out = 0;
        let result = BrotliDecompressStreamWithDictionary(
            &mut available_in,
            &mut bytes_read,
            input,
            &mut available_out,
            &mut bytes_written,
            output,
            &mut total_out,
            dictionary,
            &mut self.state,
        );
        let result = match result {
            BrotliResult::ResultFailure => 0,
            BrotliResult::ResultSuccess => 1,
            BrotliResult::NeedsMoreInput => 2,
            BrotliResult::NeedsMoreOutput => 3,
        };
        DecodeResult { result, bytes_read, bytes_written }
    }

    /// Like `BrotliDecoderGetErrorCode()`: a `BrotliDecoderErrorCode`, which
    /// is only negative for errors.
    pub fn error_code(&self) -> i32 {
        self.state.error_code as i32
    }
}
