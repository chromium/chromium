// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![allow(unsafe_code)]

use std::num::NonZero;

/// Matches C++ `content::BrowsingInstanceId`, wrapping `Option<NonZero<i32>>`
/// to idiomatically represent valid vs. invalid (`0`) IDs. With
/// `#[repr(transparent)]`, `None` has the memory bit pattern `0x00000000`,
/// which matches the invalid value for `IdType32`.
#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug, Hash)]
pub struct BrowsingInstanceId(pub Option<NonZero<i32>>);

// SAFETY: `content::BrowsingInstanceId` type is trivially movable and trivially
// destructible (the underlying structure is simply an integer) as
// enforced/documented by `browsing_instance_id_unittest.cc`.
unsafe impl cxx::ExternType for BrowsingInstanceId {
    type Id = cxx::type_id!("content::BrowsingInstanceId");
    type Kind = cxx::kind::Trivial;
}
