// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use read_fonts::{FileRef, FontRef, ReadError, TableProvider};
use skrifa::{string::StringId, MetadataProvider};
use std::fs;

fn make_font_ref_internal<'a>(font_data: &'a [u8], index: u32) -> Result<FontRef<'a>, ReadError> {
    match FileRef::new(font_data)? {
        FileRef::Font(font_ref) => Ok(font_ref),
        FileRef::Collection(collection) => collection.get(index),
    }
}

unsafe fn offset_first_table(font_bytes: &[u8]) -> u64 {
    if let Ok(font_ref) = make_font_ref_internal(font_bytes, 0) {
        // Get offset to first data table in the font file,
        // which should be somewhat equivalent to the end
        // of the TTC and OpenType font file headers.
        font_ref
            .table_directory
            .table_records()
            .iter()
            .map(|item| item.offset)
            .min()
            .unwrap_or_default()
            .get()
            .into()
    } else {
        0
    }
}

#[cxx::bridge(namespace = "name_table_access")]
pub mod ffi {

    extern "Rust" {
        unsafe fn offset_first_table(font_bytes: &[u8]) -> u64;

    }
}
