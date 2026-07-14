// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![allow(unsafe_code)]

#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug, Hash)]
pub struct ChildProcessId(pub i32);

// SAFETY: `content::ChildProcessId` type is trivially movable and trivially
// destructible (the underlying structure is simply an integer) as
// enforced/documented by `child_process_id_ffi_unittest.cc`.
unsafe impl cxx::ExternType for ChildProcessId {
    type Id = cxx::type_id!("content::ChildProcessId");
    type Kind = cxx::kind::Trivial;
}
