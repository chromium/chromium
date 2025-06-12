// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::Result;
use cxx::{CxxString, CxxVector};
use std::io::{Cursor, Read};
use std::pin::Pin;

#[cxx::bridge(namespace = "rust_zip")]
mod ffi {
    #[derive(Debug)]
    #[repr(i32)]
    enum UnzipResult {
        Success = 0,
        ErrorOpenArchive = 1,
        ErrorReadFile = 2,
    }

    struct RustFileData {
        data: Vec<u8>,
    }

    extern "Rust" {
        fn unzip_archive_in_memory(
            zip_archive_bytes: &[u8],
            out_file_contents: Pin<&mut CxxVector<RustFileData>>,
            out_error_string: Pin<&mut CxxString>,
        ) -> UnzipResult;
    }
}

fn unzip_worker(zip_archive_bytes: &[u8]) -> Result<Vec<ffi::RustFileData>> {
    let mut archive = zip::ZipArchive::new(Cursor::new(zip_archive_bytes))?;

    let mut all_files = Vec::new();

    for i in 0..archive.len() {
        let mut file_in_zip = match archive.by_index(i) {
            Ok(f) => f,
            Err(_) => {
                continue;
            }
        };

        if file_in_zip.is_dir() {
            continue;
        }

        let mut file_bytes = Vec::new();

        file_in_zip.read_to_end(&mut file_bytes)?;

        all_files.push(ffi::RustFileData { data: file_bytes });
    }

    Ok(all_files)
}

pub fn unzip_archive_in_memory(
    zip_archive_bytes: &[u8],
    mut out_file_contents: Pin<&mut CxxVector<ffi::RustFileData>>,
    mut out_error_string: Pin<&mut CxxString>,
) -> ffi::UnzipResult {
    out_error_string.as_mut().clear();

    match unzip_worker(zip_archive_bytes) {
        Ok(files) => {
            for file_data in files {
                out_file_contents.as_mut().push(file_data);
            }
            ffi::UnzipResult::Success
        }
        Err(e) => {
            out_error_string.as_mut().push_str(&e.to_string());
            ffi::UnzipResult::ErrorOpenArchive
        }
    }
}
