// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// By default, unsafe code should be disallowed, and only the FFI code below
// needs to make an exception to allow it.
//
// TODO(crbug.com/482216433): Move this to lib.rs once this crate is large
// enough to have one.
#![deny(unsafe_code)]

use cxx::UniquePtr;
use std::collections::{BTreeMap, HashMap};
use std::sync::{LazyLock, Mutex, MutexGuard};
use url::Origin;

/// This block defines the Foreign Function Interface for C++ code to call the
/// specified Rust functions. The functions operate on a
/// ChildProcessSecurityPolicyImpl singleton defined further below.
#[cxx::bridge(namespace = "content::rust::child_process_security_policy")]
mod ffi {
    #![allow(unsafe_code)]
    unsafe extern "C++" {
        include!("url/origin.rs.h");
        include!("content/browser/isolated_origin_util.h");

        // Gives us access to C++ url::Origin and all methods exposed in the origin
        // bridge file without having to redefine them here.
        #[namespace = "url"]
        type Origin = url::origin::ffi::Origin;

        #[namespace = "content"]
        type IsolatedOriginUtil;

        #[namespace = "content"]
        #[Self = "IsolatedOriginUtil"]
        #[cxx_name = "IsValidIsolatedOrigin"]
        fn is_valid_isolated_origin(origin: &Origin) -> bool;
    }

    extern "Rust" {
        fn register_web_safe_scheme(scheme: &str);
        fn register_web_safe_request_only_scheme(scheme: &str);
        fn register_pseudo_scheme(scheme: &str);
        fn is_web_safe_scheme(scheme: &str) -> bool;
        fn can_commit_scheme_in_any_process(scheme: &str) -> bool;
        fn is_pseudo_scheme(scheme: &str) -> bool;
        fn clear_registered_scheme_for_testing(scheme: &str);
        fn clear_all_registered_schemes_for_testing();

        fn add_v8_optimization_disabled_state_for_origin_if_not_cached(
            browsing_instance_id: u32,
            process_lock_origin: UniquePtr<Origin>,
            are_v8_optimizations_disabled: bool,
        );
        fn lookup_are_v8_optimizations_disabled(
            browsing_instance_id: u32,
            process_lock_origin: UniquePtr<Origin>,
            result: &mut bool,
        ) -> bool;
        fn erase_v8_optimization_state(browsing_instance_id: u32);
    }
}

// Note that there is an implicit string copy happening here: the C++ side
// passes the scheme as a std::string, which is converted into a &str by Cxx.
// This should be ok for the scheme use cases, but consider using CxxString if a
// copy is not desirable.
fn register_web_safe_scheme(scheme: &str) {
    register_scheme_internal(scheme, SchemePolicy::RequestAndCommit);
}

fn register_web_safe_request_only_scheme(scheme: &str) {
    register_scheme_internal(scheme, SchemePolicy::RequestOnly);
}

fn register_pseudo_scheme(scheme: &str) {
    register_scheme_internal(scheme, SchemePolicy::Pseudo);
}

fn is_web_safe_scheme(scheme: &str) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.known_schemes
        .get(scheme)
        .is_some_and(|p| *p == SchemePolicy::RequestOnly || *p == SchemePolicy::RequestAndCommit)
}

fn can_commit_scheme_in_any_process(scheme: &str) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.known_schemes.get(scheme).is_some_and(|p| *p == SchemePolicy::RequestAndCommit)
}

fn is_pseudo_scheme(scheme: &str) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.known_schemes.get(scheme).is_some_and(|p| *p == SchemePolicy::Pseudo)
}

fn clear_registered_scheme_for_testing(scheme: &str) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.known_schemes.remove(scheme);
}

fn clear_all_registered_schemes_for_testing() {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.known_schemes.clear();
}

fn add_v8_optimization_disabled_state_for_origin_if_not_cached(
    browsing_instance_id: u32,
    process_lock_origin: UniquePtr<Origin>,
    are_v8_optimizations_disabled: bool,
) {
    let browsing_instance_id = BrowsingInstanceId(browsing_instance_id);
    let verdict = if are_v8_optimizations_disabled {
        V8OptimizationVerdict::Disabled
    } else {
        V8OptimizationVerdict::Enabled
    };

    if !ffi::IsolatedOriginUtil::is_valid_isolated_origin(&process_lock_origin) {
        return;
    }

    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    // Get the mapping between origins and whether v8 optimization is enabled (for a
    // given browsing instance).
    let optimization_verdicts_by_origin =
        cpsp.v8_optimization_verdict_map.entry(browsing_instance_id).or_default();
    // Only store the verdict if it hasn't been cached yet.
    optimization_verdicts_by_origin.entry(process_lock_origin).or_insert(verdict);
}

/// Rust-side equivalent of LookupAreV8OptimizationsDisabled() in
/// ChildProcessSecurityPolicyImpl. The original function returns an optional
/// boolean to convey if no optimization settings are in place for a given
/// browsing instance and origin. Since CXX does not support Option<T> across
/// the boundary, we have to provide 2 booleans to the caller to convey the same
/// information. The return value is whether a result was found, and the
/// `result` parameter is for the actual optimization setting.
///
/// TODO(crbug.com/482216433): Return an optional type instead once the CXX
/// issue is fixed (or by using Crubit). See:
/// https://github.com/dtolnay/cxx/issues/87
fn lookup_are_v8_optimizations_disabled(
    browsing_instance_id: u32,
    process_lock_origin: UniquePtr<Origin>,
    result: &mut bool,
) -> bool {
    let browsing_instance_id = BrowsingInstanceId(browsing_instance_id);
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();

    // Get the mapping between origins and whether v8 optimization is enabled (for a
    // given browsing instance).
    if let Some(optimization_verdicts_by_origin) = cpsp
        .v8_optimization_verdict_map
        .get(&browsing_instance_id)
        .and_then(|map| map.get(&process_lock_origin))
    {
        *result = optimization_verdicts_by_origin == &V8OptimizationVerdict::Disabled;
        return true;
    }

    *result = false;
    false
}

fn erase_v8_optimization_state(browsing_instance_id: u32) {
    let browsing_instance_id = BrowsingInstanceId(browsing_instance_id);
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.v8_optimization_verdict_map.remove(&browsing_instance_id);
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
    /// Tracks the schemes that are ok to request or commit, or are pseudo
    /// schemes that are generally not allowed to commit.
    known_schemes: HashMap<String, SchemePolicy>,
    /// A map of BrowsingInstances and ProcessLocks (represented by their
    /// url::Origins) to v8-optimization verdicts. The purpose of the map is to
    /// ensure that changes in the return value of
    /// ContentBrowserClient::AreV8OptimizationsDisabledForSite() only affect
    /// process reuse decisions for future BrowsingInstances. The map
    /// contains a verdict for any ProcessLock eligible for disabling V8
    /// optimizations (e.g., web-safe schemes, etc) in every
    /// BrowsingInstance, and is populated at ReadyToCommit time. Stored as
    /// a BTreeMap rather than a HashMap to closer match how base::flat_map
    /// works on the C++ side (using binary search over sorted keys).
    v8_optimization_verdict_map:
        BTreeMap<BrowsingInstanceId, BTreeMap<cxx::UniquePtr<Origin>, V8OptimizationVerdict>>,
    // TODO(crbug.com/482216433): this will also eventually track per-process
    // state.
}

impl ChildProcessSecurityPolicyImpl {
    /// This is intentionally not public, because the singleton
    /// ChildProcessSecurityPolicyImpl should always be obtained via
    /// `get_locked_instance()`.
    fn new() -> Self {
        Self { known_schemes: HashMap::new(), v8_optimization_verdict_map: BTreeMap::new() }
    }

    /// Private function to get a reference to the singleton instance of
    /// ChildProcessSecurityPolicyImpl, wrapping it in a Mutex for thread
    /// safety. This is initialized lazily on first use, and the `LazyLock`
    /// ensures that the `ChildProcessSecurityPolicyImpl` is initialized
    /// exactly once across all threads. The `Mutex` must be acquired before
    /// any internal security state is read or modified.
    ///
    /// NOTE: Unlike the C++ implementation, which uses multiple fine-grained
    /// locks (e.g., `lock_`, `isolated_origins_lock_`,
    /// `origins_isolation_opt_in_lock_`) to reduce thread contention, the
    /// Rust implementation deliberately uses a single class-wide Mutex.
    /// This simplifies the concurrency model and reduces the risk of
    /// lock-ordering deadlocks.
    fn get_instance() -> &'static Mutex<ChildProcessSecurityPolicyImpl> {
        static INSTANCE: LazyLock<Mutex<ChildProcessSecurityPolicyImpl>> =
            LazyLock::new(|| Mutex::new(ChildProcessSecurityPolicyImpl::new()));
        &INSTANCE
    }

    /// Helper to retrieve the global ChildProcessSecurityPolicyImpl and then
    /// acquire the Mutex.
    ///
    /// Note that this is not public. Instead, the public API for
    /// ChildProcessSecurityPolicyImpl is provided by the FFI functions above,
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

/// An identifier for a `BrowsingInstance`, matching the C++ side
/// `BrowsingInstanceId`.
// TODO(crbug.com/519701929): Add FFI for BrowsingInstanceId so one definition
// can be used by both Rust and C++.
#[derive(PartialEq, Eq, PartialOrd, Ord)]
pub struct BrowsingInstanceId(u32);

/// An enum tracking whether v8 optimizations are enabled or disabled.
#[derive(PartialEq, Eq)]
enum V8OptimizationVerdict {
    Enabled,
    Disabled,
}

/// Represents what behavior is allowed for a given known scheme.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
enum SchemePolicy {
    /// Schemes that are ok to request from any renderer process. This includes
    /// both web-safe and web-safe isolated schemes.
    RequestOnly,
    /// Schemes that are ok to commit in any renderer process, which are also ok
    /// to request. This includes web-safe schemes but not web-safe isolated
    /// schemes.
    RequestAndCommit,
    /// Pseudo schemes do not actually represent retrievable URLs. For example,
    /// most of the URLs in the `about` scheme (apart from `about:blank` and
    /// `about:srcdoc`) are aliases to other URLs. Thus, `about` is registered
    /// as a pseudo scheme, with exceptions made to allow `about:blank` and
    /// `about:srcdoc` to commit.
    Pseudo,
}

/// Helper function to track how a given scheme should be treated, without
/// allowing duplicate registrations.
fn register_scheme_internal(scheme: &str, policy: SchemePolicy) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(old_policy) = cpsp.known_schemes.insert(scheme.to_string(), policy) {
        panic!("Scheme {scheme:?} is already registered as {old_policy:?}");
    }
}
