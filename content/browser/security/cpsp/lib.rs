// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// By default, unsafe code should be disallowed, and only the FFI code in the
// modules needs to make an exception to allow it.
#![deny(unsafe_code)]

chromium::import! {
    "//base:unguessable_token";
}

mod child_process_security_policy_impl;
mod process_state;

pub(crate) use child_process_security_policy_impl::ChildProcessSecurityPolicyImpl;
