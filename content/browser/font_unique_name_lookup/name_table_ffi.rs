// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use read_fonts::{FileRef, FontRef, ReadError};
use skrifa::{string::StringId, MetadataProvider};

fn make_font_ref_internal<'a>(font_data: &'a [u8], index: u32) -> Result<FontRef<'a>, ReadError> {
    match FileRef::new(font_data)? {
        FileRef::Font(font_ref) => Ok(font_ref),
        FileRef::Collection(collection) => collection.get(index),
    }
}

fn english_unique_font_names<'a>(font_bytes: &[u8], index: u32) -> Vec<String> {
    if let Ok(font_ref) = make_font_ref_internal(font_bytes, index) {
        let mut return_vec = Vec::new();
        for id in [StringId::FULL_NAME, StringId::POSTSCRIPT_NAME] {
            if let Some(font_name) = font_ref.localized_strings(id).english_or_first() {
                let name_added = font_name.to_string();
                return_vec.push(name_added);
            }
        }
        return_vec
    } else {
        Vec::new()
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

fn indexable_num_fonts<'a>(font_bytes: &[u8]) -> u32 {
    let maybe_font_or_collection = FileRef::new(font_bytes);
    match maybe_font_or_collection {
        Ok(FileRef::Collection(collection)) => collection.len(),
        Ok(FileRef::Font(_)) => 1u32,
        _ => 0u32,
    }
}

#[cxx::bridge(namespace = "name_table_access")]
pub mod ffi {
    extern "Rust" {
        /// Returns true if the font or font collection is indexable and gives
        /// the number of fonts contained in font or collection. 1 is
        /// ambiguous and means either a single font file
        /// or a collection, but since `english_unique_font_names` ignore the
        /// argument if the font is not a collection, this is ok.
        unsafe fn indexable_num_fonts<'a>(font_bytes: &[u8]) -> u32;
        unsafe fn english_unique_font_names<'a>(font_bytes: &[u8], index: u32) -> Vec<String>;
        unsafe fn offset_first_table(font_bytes: &[u8]) -> u64;
    }
}
