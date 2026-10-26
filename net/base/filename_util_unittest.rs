// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//net/base:filename_util";
    "//url:url_rust";
}

use filename_util::file_url_to_file_path;
use rust_gtest_interop::prelude::*;
use std::path::Path;
use url_rust::gurl::ffi::create_gurl_from_str;

#[gtest(FilenameUtilRustTest, FileUrlToFilePath)]
fn test_file_url_to_file_path() {
    #[cfg(unix)]
    {
        let file_url = create_gurl_from_str("file:///foo/bar/baz.txt");
        let path = file_url_to_file_path(&file_url).expect("Expected valid file path");
        expect_eq!(path.as_path(), Path::new("/foo/bar/baz.txt"));
    }

    #[cfg(windows)]
    {
        let file_url = create_gurl_from_str("file:///C:/foo/bar/baz.txt");
        let path = file_url_to_file_path(&file_url).expect("Expected valid file path");
        expect_eq!(path.to_path_buf(), Path::new(r"C:\foo\bar\baz.txt"));
    }

    // Non-file URLs should fail.
    let https_url = create_gurl_from_str("https://example.com/foo.txt");
    expect_true!(file_url_to_file_path(&https_url).is_none());

    // Invalid URLs should fail.
    let invalid_url = create_gurl_from_str("not a url");
    expect_true!(file_url_to_file_path(&invalid_url).is_none());

    // Encoded slash (%2F) or null byte (%00) should fail.
    let encoded_slash_url = create_gurl_from_str("file:///foo%2fbar.txt");
    expect_true!(file_url_to_file_path(&encoded_slash_url).is_none());

    let encoded_null_url = create_gurl_from_str("file:///foo/%00bar.txt");
    expect_true!(file_url_to_file_path(&encoded_null_url).is_none());
}
