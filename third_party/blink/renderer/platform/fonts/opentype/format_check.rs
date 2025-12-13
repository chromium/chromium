// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use font_types::Tag;
use read_fonts::{FileRef, FontRef, ReadError, TableProvider};

fn make_font_ref_internal<'a>(font_data: &'a [u8], index: u32) -> Result<FontRef<'a>, ReadError> {
    match FileRef::new(font_data)? {
        FileRef::Font(font_ref) => Ok(font_ref),
        FileRef::Collection(collection) => collection.get(index),
    }
}

#[allow(unused)]
pub struct FontFormatFlags {
    table_tags: Vec<Tag>,
    color_version: Option<u16>,
    avar_version: Option<(u16, u16)>,
}

#[derive(Default)]
pub struct FontFormatInfo {
    format_flags: Option<FontFormatFlags>,
}

pub fn get_font_format_info<'a>(font_bytes: &'a [u8]) -> Box<FontFormatInfo> {
    let file_ref = make_font_ref_internal(font_bytes, 0);

    match file_ref {
        Ok(font) => {
            let table_tags =
                font.table_directory().table_records().into_iter().map(|e| e.tag()).collect();
            let color_version = get_colr_version(&font);
            let avar_version = get_avar_version(&font);
            Box::new(FontFormatInfo {
                format_flags: Some(FontFormatFlags { table_tags, color_version, avar_version }),
            })
        }
        _ => Box::new(FontFormatInfo::default()),
    }
}

fn get_colr_version(font_ref: &FontRef) -> Option<u16> {
    Some(font_ref.colr().ok()?.version())
}

fn is_colrv1(format_info: &FontFormatInfo) -> bool {
    match &format_info.format_flags {
        Some(FontFormatFlags { color_version: Some(1), .. }) => true,
        _ => false,
    }
}
fn is_colrv0(format_info: &FontFormatInfo) -> bool {
    match &format_info.format_flags {
        Some(FontFormatFlags { color_version: Some(0), .. }) => true,
        _ => false,
    }
}

fn get_avar_version(font_ref: &FontRef) -> Option<(u16, u16)> {
    let version = font_ref.avar().ok()?.version();
    Some((version.major, version.minor))
}

fn is_avar2(format_info: &FontFormatInfo) -> bool {
    match &format_info.format_flags {
        Some(FontFormatFlags { avar_version: Some((2, _)), .. }) => true,
        _ => false,
    }
}

fn has_tags(format_info: &FontFormatInfo, query: &[Tag]) -> bool {
    match &format_info.format_flags {
        Some(FontFormatFlags { table_tags, .. }) => {
            query.iter().all(|tag| table_tags.contains(tag))
        }
        _ => false,
    }
}

fn is_variable(format_info: &FontFormatInfo) -> bool {
    has_tags(format_info, &[Tag::new(b"fvar")])
}

fn is_sbix(format_info: &FontFormatInfo) -> bool {
    has_tags(format_info, &[Tag::new(b"sbix")])
}

fn is_cbdt_cblc(format_info: &FontFormatInfo) -> bool {
    has_tags(format_info, &[Tag::new(b"CBDT"), Tag::new(b"CBLC")])
}

fn is_cff2(format_info: &FontFormatInfo) -> bool {
    has_tags(format_info, &[Tag::new(b"CFF2")])
}

#[cxx::bridge(namespace = "font_format_check")]
#[allow(unused_unsafe)]
pub mod ffi {
    extern "Rust" {
        type FontFormatInfo;
        unsafe fn get_font_format_info<'a>(font_bytes: &'a [u8]) -> Box<FontFormatInfo>;
        fn is_colrv1(format_info: &FontFormatInfo) -> bool;
        fn is_colrv0(format_info: &FontFormatInfo) -> bool;
        fn is_cbdt_cblc(format_info: &FontFormatInfo) -> bool;
        fn is_variable(format_info: &FontFormatInfo) -> bool;
        fn is_sbix(format_info: &FontFormatInfo) -> bool;
        fn is_cff2(format_info: &FontFormatInfo) -> bool;
        fn is_avar2(format_info: &FontFormatInfo) -> bool;
    }
}
