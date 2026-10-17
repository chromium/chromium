// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Contains the conversion definitions for mapping the `ReadOnlyFile` mojom
//! type to and from a `base::File`.

chromium::import! {
    pub "//base:file" as base_file;
    "//mojo/public/rust/base:read_only_file_checks";
    "//mojo/public/rust/c_mojo_api";
}

use anyhow::Context;
use c_mojo_api::platform::{
    MojoUnwrapPlatformHandle, MojoWrapPlatformHandle, OwnedPlatformHandle, PlatformHandle,
};

// Note that since this is a typemapping file, `super` refers to the generated
// mojom crate, so `ReadOnlyFile` is the generated mojom type.
use super::ReadOnlyFile;

impl From<base_file::File> for ReadOnlyFile {
    fn from(file: base_file::File) -> Self {
        // These checks mirror the C++ serializer in
        // //mojo/public/cpp/base/read_only_file_mojom_traits.cc
        assert!(file.is_valid(), "Cannot serialize an invalid base::File as ReadOnlyFile");
        assert!(
            read_only_file_checks::is_read_only_file(&file),
            "Cannot serialize a writable base::File as ReadOnlyFile"
        );
        // Checking for a physical file may block, so only do it
        // when DCHECKs are on, same as C++.
        debug_assert!(
            read_only_file_checks::is_physical_file(&file),
            "ReadOnlyFile only supports physically-backed files"
        );

        let handle = OwnedPlatformHandle::try_from(file)
            .expect("Cannot serialize an invalid base::File as ReadOnlyFile");
        let fd = MojoWrapPlatformHandle(PlatformHandle::from(handle))
            .expect("Failed to wrap platform handle");
        Self { fd, r#async: false }
    }
}

impl TryFrom<ReadOnlyFile> for base_file::File {
    type Error = anyhow::Error;

    fn try_from(mojom: ReadOnlyFile) -> anyhow::Result<Self> {
        if mojom.r#async {
            anyhow::bail!("Async base::File is not supported in Rust");
        }
        let handle = MojoUnwrapPlatformHandle(mojom.fd)
            .context("ReadOnlyFile did not contain a platform handle")?;
        let file = Self::from(OwnedPlatformHandle::from(handle));
        if !read_only_file_checks::is_read_only_file(&file) {
            anyhow::bail!("ReadOnlyFile contained a writable handle");
        }
        Ok(file)
    }
}
