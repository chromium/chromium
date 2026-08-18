// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub mod cpp_router_handle;
pub mod cxx;
pub use cxx::ffi;

pub use cpp_router_handle::{CppResponseSender, CppRouterHandle};

/// A type representing a pending associated endpoint in C++, which can
/// be used to create a Rust associated endpoint that sends its messages
/// through a C++ pipe.
pub type CxxPendingAssociatedEndpoint = ffi::AssociatedEndpointRustAdapter;
