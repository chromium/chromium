// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Traits and related types implemented by generated code and used by generic
//! mojo code.

/// A type which uniquely identifies a mojom-defined interface.
///
/// Types implementing this currently serve as marker types, rather than their
/// instances having functionality.
pub trait Interface {
    fn get_method_info(name: u32) -> Option<&'static MethodInfo>;
}

pub type MethodValidator =
    for<'a, 'b, 'c> fn(&'c mut crate::ValidationContext<'a, 'b>) -> crate::Result<()>;

/// Static data for a particular method on an interface.
///
/// Contains data required to interpret messages for this method. Static
/// instances are generated as a part of mojom bindings.
pub struct MethodInfo {
    pub validate_request: MethodValidator,
    pub validate_response: Option<MethodValidator>,
}
