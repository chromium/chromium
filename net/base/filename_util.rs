// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:file_path";
    "//url:url_rust";
}

use cxx::UniquePtr;
use file_path::ffi::FilePath;
use url_rust::GURL;

#[cxx::bridge(namespace = "net::rust")]
mod ffi {
    unsafe extern "C++" {
        include!("base/files/file_path.h");
        include!("net/base/filename_util_rust_shim.h");
        include!("url/gurl.h");

        #[namespace = "base"]
        type FilePath = super::file_path::ffi::FilePath;

        #[namespace = ""]
        type GURL = super::url_rust::gurl::ffi::GURL;

        #[rust_name = "file_url_to_file_path"]
        fn FileURLToFilePath(url: &GURL) -> UniquePtr<FilePath>;
    }
}

/// Converts a `file:` `GURL` back to a `FilePath` that can be passed to the OS.
/// Returns `Some(FilePath)` on success, or `None` if `url` is invalid or the
/// file path cannot be extracted from `url`.
pub fn file_url_to_file_path(url: &GURL) -> Option<UniquePtr<FilePath>> {
    let path = ffi::file_url_to_file_path(url);
    if path.is_null() {
        None
    } else {
        Some(path)
    }
}
