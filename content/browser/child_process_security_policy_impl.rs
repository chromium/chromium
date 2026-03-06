// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// By default, unsafe code should be disallowed, and only the FFI code below
// needs to make an exception to allow it.
//
// TODO(crbug.com/482216433): Move this to lib.rs once this crate is large
// enough to have one.
#![deny(unsafe_code)]

use std::collections::HashSet;
use std::sync::{LazyLock, Mutex, MutexGuard};

/// This block defines the Foreign Function Interface for C++ code to call the
/// specified Rust functions. The functions operate on a
/// ChildProcessSecurityPolicyImpl singleton defined further below.
#[cxx::bridge(namespace = "content::rust::child_process_security_policy")]
mod ffi {
    #![allow(unsafe_code)]
    extern "Rust" {
        fn register_web_safe_scheme(scheme: &str);
        fn register_web_safe_request_only_scheme(scheme: &str);
        fn is_web_safe_scheme(scheme: &str) -> bool;
        fn can_commit_scheme_in_any_process(scheme: &str) -> bool;
        fn clear_web_safe_scheme_for_testing(scheme: &str);
    }
}

// Note that there is an implicit string copy happening here: the C++ side
// passes the scheme as a std::string, which is converted into a &str by Cxx.
// This should be ok for the scheme use cases, but consider using CxxString if a
// copy is not desirable.
fn register_web_safe_scheme(scheme: &str) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.schemes_ok_to_request_in_any_process.insert(scheme.to_string());
    cpsp.schemes_ok_to_commit_in_any_process.insert(scheme.to_string());
}
fn register_web_safe_request_only_scheme(scheme: &str) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.schemes_ok_to_request_in_any_process.insert(scheme.to_string());
}
fn is_web_safe_scheme(scheme: &str) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.schemes_ok_to_request_in_any_process.contains(scheme)
}
fn can_commit_scheme_in_any_process(scheme: &str) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.schemes_ok_to_commit_in_any_process.contains(scheme)
}
fn clear_web_safe_scheme_for_testing(scheme: &str) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.schemes_ok_to_request_in_any_process.remove(scheme);
    cpsp.schemes_ok_to_commit_in_any_process.remove(scheme);
}

/// Defines a global policy object that tracks security information for child
/// processes as well as global security state. This is intended to primarily be
/// used for access checks on renderer processes but may eventually be used for
/// other kinds of processes that are hosting untrustworthy code, such as
/// utility processes.
///
/// This object supports being accessed from different threads and guards access
/// to its internal data with a Mutex.
pub struct ChildProcessSecurityPolicyImpl {
    /// Tracks the list of web-safe schemes that are ok to request from any
    /// renderer process.
    schemes_ok_to_request_in_any_process: HashSet<String>,
    /// Tracks the list of schemes that are ok to commit in any renderer
    /// process. These are generally a subset of
    /// `schemes_ok_to_request_in_any_process`.
    schemes_ok_to_commit_in_any_process: HashSet<String>,
    // TODO(crbug.com/482216433): this will also eventually track per-process
    // state.
}
impl ChildProcessSecurityPolicyImpl {
    /// This is intentionally not public, because the singleton
    /// ChildProcessSecurityPolicyImpl should always be obtained via
    /// `get_locked_instance()`.
    fn new() -> Self {
        Self {
            schemes_ok_to_request_in_any_process: HashSet::new(),
            schemes_ok_to_commit_in_any_process: HashSet::new(),
        }
    }
    /// Private function to get a reference to the singleton instance of
    /// ChildProcessSecurityPolicyImpl, wrapping it in a Mutex for thread
    /// safety. This is initialized lazily on first use, and the `LazyLock`
    /// ensures that the `ChildProcessSecurityPolicyImpl` is initialized exactly
    /// once across all threads. The `Mutex` must be acquired before any
    /// internal security state is read or modified.
    fn get_instance() -> &'static Mutex<ChildProcessSecurityPolicyImpl> {
        static INSTANCE: LazyLock<Mutex<ChildProcessSecurityPolicyImpl>> =
            LazyLock::new(|| Mutex::new(ChildProcessSecurityPolicyImpl::new()));
        &INSTANCE
    }
    /// Helper to retrieve the global ChildProcessSecurityPolicyImpl and then
    /// acquire the Mutex.
    ///
    /// Note that this is not public. Instead, the public API for
    /// ChildProcessSecurityPolicyImpl is provided by the FFI functions below,
    /// which use this to operate on the underlying
    /// ChildProcessSecurityPolicyImpl.
    fn get_locked_instance() -> MutexGuard<'static, ChildProcessSecurityPolicyImpl> {
        // `unwrap` is ok, because Chromium is always built with `-Cpanic=abort`
        // which means that a Mutex cannot be poisoned when unwinding a panic
        // while holding the mutex.
        //
        // TODO(crbug.com/477584253): Consider switching this to use
        // std::sync::nonpoison::Mutex once it is stabilized.
        Self::get_instance().lock().unwrap()
    }
}
