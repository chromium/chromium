// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Exposes the C++ checks used to validate `mojo_base.mojom.ReadOnlyFile`
//! handles, so that the Rust typemap in `read_only_file_mojom_traits.rs` can
//! enforce the same invariants as the C++ `StructTraits`.

chromium::import! {
    "//base:file" as base_file;
}

#[cxx::bridge(namespace = "mojo")]
mod ffi {
    unsafe extern "C++" {
        include!("base/files/file.h");
        include!("mojo/public/cpp/base/read_only_file_mojom_traits.h");

        // This tells `cxx` that the type already appears in another bridge,
        // so it just needs to import the existing bindings.
        #[namespace = "base"]
        type File = base_file::ffi::File;

        fn IsReadOnlyFile(file: &File) -> bool;
        fn IsPhysicalFile(file: &File) -> bool;
    }
}

/// Returns true if `file` cannot be used to modify the underlying file. On
/// platforms where the handle cannot be inspected, this always returns true.
pub fn is_read_only_file(file: &base_file::File) -> bool {
    ffi::IsReadOnlyFile(file.as_ref())
}

/// Returns true if `file` is backed by a physical file rather than e.g. a
/// socket or a pipe. This may block, so use it carefully.
pub fn is_physical_file(file: &base_file::File) -> bool {
    ffi::IsPhysicalFile(file.as_ref())
}
