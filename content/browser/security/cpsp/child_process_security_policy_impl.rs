// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cxx::UniquePtr;
use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::sync::{LazyLock, Mutex, MutexGuard};
use storage_common::FileSystemType;
use url::Origin;

use content_browser_id_types::BrowsingInstanceId;

use crate::process_state::ProcessStateMaps;

/// This block defines the Foreign Function Interface for C++ code to call the
/// specified Rust functions. The functions operate on a
/// ChildProcessSecurityPolicyImpl singleton defined further below.
#[cxx::bridge(namespace = "content::rust::child_process_security_policy")]
mod ffi {
    #![allow(unsafe_code)]
    unsafe extern "C++" {
        include!("url/origin.rs.h");
        include!("content/browser/isolated_origin_util.h");
        include!("storage/common/file_system/file_system_types.h");
        include!("content/public/browser/browsing_instance_id.h");

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

        #[namespace = "content"]
        #[Self = "IsolatedOriginUtil"]
        #[cxx_name = "IsValidOriginForOriginAgentClusterOptIn"]
        fn is_valid_origin_for_origin_agent_cluster_opt_in(origin: &Origin) -> bool;

        #[namespace = "content"]
        #[Self = "IsolatedOriginUtil"]
        #[cxx_name = "IsValidOriginForOriginAgentClusterOptOut"]
        fn is_valid_origin_for_origin_agent_cluster_opt_out(origin: &Origin) -> bool;

        #[namespace = "storage"]
        type FileSystemType = storage_common::FileSystemType;

        #[namespace = "content"]
        type BrowsingInstanceId = content_browser_id_types::BrowsingInstanceId;
    }

    extern "Rust" {
        // Global state APIs
        fn register_web_safe_scheme(scheme: &str);
        fn register_web_safe_request_only_scheme(scheme: &str);
        fn register_pseudo_scheme(scheme: &str);
        fn is_web_safe_scheme(scheme: &str) -> bool;
        fn can_commit_scheme_in_any_process(scheme: &str) -> bool;
        fn is_pseudo_scheme(scheme: &str) -> bool;
        fn clear_registered_scheme_for_testing(scheme: &str);
        fn clear_all_registered_schemes_for_testing();

        fn add_v8_optimization_disabled_state_for_origin_if_not_cached(
            browsing_instance_id: BrowsingInstanceId,
            process_lock_origin: UniquePtr<Origin>,
            are_v8_optimizations_disabled: bool,
        );
        fn lookup_are_v8_optimizations_disabled(
            browsing_instance_id: BrowsingInstanceId,
            process_lock_origin: UniquePtr<Origin>,
            result: &mut bool,
        ) -> bool;
        fn erase_v8_optimization_state(browsing_instance_id: BrowsingInstanceId);

        fn register_file_system_permission_policy(file_system_type: FileSystemType, policy: i32);
        fn find_permissions_for_file_system_type(
            file_system_type: FileSystemType,
            policy: &mut i32,
        ) -> bool;

        fn record_origin_agent_cluster_request_if_new(
            browser_context_id: &str,
            origin: UniquePtr<Origin>,
        ) -> bool;
        fn has_origin_ever_requested_origin_agent_cluster_value(
            browser_context_id: &str,
            origin: UniquePtr<Origin>,
        ) -> bool;
        fn remove_origin_agent_cluster_requests_for_browser_context(browser_context_id: &str);

        fn lookup_origin_agent_cluster_state(
            browsing_instance_id: BrowsingInstanceId,
            origin: UniquePtr<Origin>,
            result: &mut OriginAgentClusterIsolationState,
        ) -> bool;
        fn add_origin_agent_cluster_state_for_browsing_instance(
            browsing_instance_id: BrowsingInstanceId,
            origin: UniquePtr<Origin>,
            oac_state: OriginAgentClusterIsolationState,
            default_oac_state: OriginAgentClusterIsolationState,
        );
        fn record_default_origin_agent_cluster_origin_if_new(
            browsing_instance_id: BrowsingInstanceId,
            browser_context_id: &str,
            origin: UniquePtr<Origin>,
            oac_state: OriginAgentClusterIsolationState,
            is_global_walk_or_frame_removal: bool,
        );
        fn erase_origin_agent_cluster_state(browsing_instance_id: BrowsingInstanceId);
    }

    // Tracks the state of an Origin-Agent-Cluster request for a particular
    // origin. The Origin-Agent-Cluster header can be used to request either an
    // origin-keyed agent cluster (?1) or a site-keyed one (?0).
    //
    // This enum combines two distinct forms of isolation:
    // 1. Logical isolation: Whether the agent cluster is origin-keyed in the
    //    renderer process, affecting web-visible behavior (e.g. document.domain).
    //    In the absence of an OAC header, this defaults to origin-keyed if
    //    blink::features::kOriginAgentClusterDefaultEnabled is enabled, and
    //    site-keyed otherwise.
    // 2. Process isolation: Whether the origin requires an origin-keyed process in
    //    the process model. In the absence of an OAC header, this defaults to an
    //    origin-keyed process if features::kOriginKeyedProcessesByDefault is
    //    enabled, and a site-keyed process otherwise. If process isolation is true,
    //    logical isolation must also be true.
    //
    // In the C++ `content::OriginAgentClusterIsolationState` class, these two
    // forms are tracked using two separate `AgentClusterKey::OACStatus` fields.
    // In Rust, we collapse the valid combinations of those two fields into this
    // enum to guarantee that invalid states (like process isolation without
    // logical isolation) are structurally impossible to represent.
    #[derive(Debug, PartialEq, Eq)]
    enum OriginAgentClusterIsolationState {
        /// Site-keyed agent cluster and process, applied by default.
        SiteKeyedByDefault,
        /// Site-keyed agent cluster and process, explicitly requested via
        /// OAC: ?0 (opt-out) header.
        SiteKeyedByHeader,
        /// Origin-keyed logically (renderer-side), but site-keyed in the
        /// process model. Applied by default.
        OriginKeyedLogicalOnlyByDefault,
        /// Origin-keyed logically (renderer-side), but site-keyed in the
        /// browser process model. Explicitly requested via OAC: ?1 (opt-in)
        /// header.
        OriginKeyedLogicalOnlyByHeader,
        /// Origin-keyed logically and process-isolated. Applied by default.
        /// Valid only when
        /// SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault()
        /// returns true.
        OriginKeyedProcessIsolatedByDefault,
        /// Origin-keyed logically and process-isolated. Explicitly requested
        /// via OAC: ?1 (opt-in) header. Valid only when
        /// SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled()
        /// returns true.
        OriginKeyedProcessIsolatedByHeader,
    }
}

impl ffi::OriginAgentClusterIsolationState {
    pub fn is_origin_keyed_agent_cluster_by_header(&self) -> bool {
        matches!(
            *self,
            ffi::OriginAgentClusterIsolationState::OriginKeyedLogicalOnlyByHeader
                | ffi::OriginAgentClusterIsolationState::OriginKeyedProcessIsolatedByHeader
        )
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
    browsing_instance_id: BrowsingInstanceId,
    process_lock_origin: UniquePtr<Origin>,
    are_v8_optimizations_disabled: bool,
) {
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
    browsing_instance_id: BrowsingInstanceId,
    process_lock_origin: UniquePtr<Origin>,
    result: &mut bool,
) -> bool {
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

fn erase_v8_optimization_state(browsing_instance_id: BrowsingInstanceId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.v8_optimization_verdict_map.remove(&browsing_instance_id);
}

fn register_file_system_permission_policy(file_system_type: FileSystemType, policy: i32) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.file_system_policy_map.insert(file_system_type, policy);
}

fn find_permissions_for_file_system_type(
    file_system_type: FileSystemType,
    policy: &mut i32,
) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(val) = cpsp.file_system_policy_map.get(&file_system_type) {
        *policy = *val;
        return true;
    }
    false
}

fn record_origin_agent_cluster_request_if_new(
    browser_context_id: &str,
    origin: UniquePtr<ffi::Origin>,
) -> bool {
    if !ffi::IsolatedOriginUtil::is_valid_origin_for_origin_agent_cluster_opt_in(&origin) {
        return false;
    }

    let browser_context_id = BrowserContextId(browser_context_id.to_string());
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    let origins = cpsp.origin_agent_cluster_opt_ins_and_outs.entry(browser_context_id).or_default();

    if origins.contains(&origin) {
        return false;
    }

    origins.insert(origin);
    true
}

fn has_origin_ever_requested_origin_agent_cluster_value(
    browser_context_id: &str,
    origin: UniquePtr<ffi::Origin>,
) -> bool {
    let browser_context_id = BrowserContextId(browser_context_id.to_string());
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.origin_agent_cluster_opt_ins_and_outs
        .get(&browser_context_id)
        .is_some_and(|origins| origins.contains(&origin))
}

fn remove_origin_agent_cluster_requests_for_browser_context(browser_context_id: &str) {
    let browser_context_id = BrowserContextId(browser_context_id.to_string());
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.origin_agent_cluster_opt_ins_and_outs.remove(&browser_context_id);
}

fn lookup_origin_agent_cluster_state(
    browsing_instance_id: BrowsingInstanceId,
    origin: UniquePtr<ffi::Origin>,
    result: &mut ffi::OriginAgentClusterIsolationState,
) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(oac_state) = cpsp
        .origin_agent_cluster_states_by_browsing_instance
        .get(&browsing_instance_id)
        .and_then(|map| map.get(&origin))
    {
        *result = *oac_state;
        return true;
    }
    false
}

fn add_origin_agent_cluster_state_for_browsing_instance(
    browsing_instance_id: BrowsingInstanceId,
    origin: UniquePtr<ffi::Origin>,
    oac_state: ffi::OriginAgentClusterIsolationState,
    default_oac_state: ffi::OriginAgentClusterIsolationState,
) {
    // We should only be registering an isolation state if it deviates from the
    // default isolation state (e.g., if it's explicitly requested by a header or
    // if an ad frame's process isolation is being bypassed).
    assert!(
        oac_state != default_oac_state,
        "Trying to add invalid OAC state: {:?} (default: {:?})",
        oac_state,
        default_oac_state
    );

    let is_valid_opt_in = oac_state.is_origin_keyed_agent_cluster_by_header()
        && ffi::IsolatedOriginUtil::is_valid_origin_for_origin_agent_cluster_opt_in(&origin);

    // This check is specific to OAC-by-default, and is required to allow
    // explicit opt-outs for HTTP-schemed origins. See
    // OriginAgentClusterInsecureEnabledBrowserTest.DocumentDomain_Disabled.
    let is_valid_opt_out =
        ffi::IsolatedOriginUtil::is_valid_origin_for_origin_agent_cluster_opt_out(&origin);

    // We ought to have validated the origin prior to getting here.  If the origin
    // isn't valid at this point, something has gone wrong.
    assert!(is_valid_opt_in || is_valid_opt_out, "Trying to isolate invalid origin: {:?}", *origin);

    assert_ne!(browsing_instance_id, BrowsingInstanceId(None));

    // Register the OAC state for `origin` in the per-BrowsingInstance map. We
    // only support adding new entries, not modifying existing ones. If at some
    // point in the future we allow isolation state to change during the
    // lifetime of a BrowsingInstance, then this will need to be updated.
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    let states = cpsp
        .origin_agent_cluster_states_by_browsing_instance
        .entry(browsing_instance_id)
        .or_default();

    states.entry(origin).or_insert(oac_state);
}

fn record_default_origin_agent_cluster_origin_if_new(
    browsing_instance_id: BrowsingInstanceId,
    browser_context_id: &str,
    origin: UniquePtr<ffi::Origin>,
    oac_state: ffi::OriginAgentClusterIsolationState,
    is_global_walk_or_frame_removal: bool,
) {
    if !ffi::IsolatedOriginUtil::is_valid_origin_for_origin_agent_cluster_opt_in(&origin) {
        return;
    }

    assert_ne!(browsing_instance_id, BrowsingInstanceId(None));

    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();

    // Commits of origins that have ever sent the OriginAgentCluster header in
    // this BrowserContext are tracked in every BrowsingInstance in this
    // BrowserContext, to avoid having to do multiple global walks. If the origin
    // isn't in the list of such origins (i.e., the common case), return early to
    // avoid unnecessary work, since this is called on every commit. Skip this
    // during global walks and frame removals, since we do want to track the
    // origin's non-isolated status in those cases.
    if !is_global_walk_or_frame_removal {
        let browser_context_id = BrowserContextId(browser_context_id.to_string());
        let has_ever_requested_oac = cpsp
            .origin_agent_cluster_opt_ins_and_outs
            .get(&browser_context_id)
            .is_some_and(|origins| origins.contains(&origin));
        if !has_ever_requested_oac {
            return;
        }
    }

    let states = cpsp
        .origin_agent_cluster_states_by_browsing_instance
        .entry(browsing_instance_id)
        .or_default();

    // If `origin` has already recorded an Origin-Agent-Cluster state, then we
    // don't want to add it to the list. Technically this check is unnecessary
    // during global walks (when the origin won't be in this list yet), but it
    // matters during frame removal (when we don't want to add an opted-in
    // origin to the list as non-isolated when its frame is removed).
    if states.contains_key(&origin) {
        return;
    }

    states.insert(origin, oac_state);
}

fn erase_origin_agent_cluster_state(browsing_instance_id: BrowsingInstanceId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.origin_agent_cluster_states_by_browsing_instance.remove(&browsing_instance_id);
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
    // Tracks all per-process security states, including while the
    // RenderProcessHost exists and the state can be modified, and after it has
    // been deleted until all of the ChildProcessSecurityPolicy::Handles are
    // gone (when the state can be queried but not modified).
    pub(crate) process_states: ProcessStateMaps,

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

    /// A map of FileSystemTypes to bitwise-or'd combinations of permission
    /// policies allowed for those types. See
    /// storage::FileSystemContext::GetPermissionPolicy.
    file_system_policy_map: BTreeMap<FileSystemType, i32>,

    // The set of all origins that have ever explicitly requested an
    // Origin-Agent-Cluster state (either opting in or opting out), organized by
    // BrowserContext ID. This allows us to know which origins need to be
    // tracked when using default isolation in any given BrowsingInstance.
    // Origins requesting an Origin-Agent-Cluster state, if successful, are
    // marked as isolated or not via `DetermineOriginAgentClusterIsolation()`.
    // Each BrowserContext's state is tracked separately so that timing attacks
    // do not reveal whether an origin has been visited in another (e.g.,
    // incognito) BrowserContext. In general, the state of other
    // BrowsingInstances is not observable outside such timing side channels.
    //
    // Stored as a BTreeMap rather than a HashMap to closer match how
    // base::flat_map works on the C++ side (using binary search over sorted
    // keys).
    origin_agent_cluster_opt_ins_and_outs:
        BTreeMap<BrowserContextId, BTreeSet<cxx::UniquePtr<ffi::Origin>>>,

    // A map to track origins that have been isolated via Origin-Agent-Cluster
    // within a given BrowsingInstance, or that have been loaded in a
    // BrowsingInstance without isolation, but that have requested an
    // Origin-Agent-Cluster state in at least one other BrowsingInstance.
    // Origins loaded without isolation are tracked to make sure we don't try to
    // isolate the origin in the associated BrowsingInstance at a later time, in
    // order to keep the isolation consistent over the lifetime of the
    // BrowsingInstance.
    //
    // Note that the origins passed into this map are currently derived directly
    // from the URL, and are not the actual origins that commit. Because of
    // this, this map does not distinguish between a non-sandboxed origin and an
    // opaque sandboxed origin that shares the same precursor. Consequently, if
    // a sandboxed frame and a regular frame from the same origin coexist in a
    // BrowsingInstance, they are forced to share the same OAC tracking state.
    // Ideally, they should be tracked independently since they are distinct
    // origins that cannot script each other. See https://crbug.com/40910871 and
    // https://crbug.com/446157743.
    origin_agent_cluster_states_by_browsing_instance: BTreeMap<
        BrowsingInstanceId,
        BTreeMap<cxx::UniquePtr<ffi::Origin>, ffi::OriginAgentClusterIsolationState>,
    >,
}

impl ChildProcessSecurityPolicyImpl {
    /// This is intentionally not public, because the singleton
    /// ChildProcessSecurityPolicyImpl should always be obtained via
    /// `get_locked_instance()`.
    fn new() -> Self {
        Self {
            process_states: ProcessStateMaps::new(),
            known_schemes: HashMap::new(),
            v8_optimization_verdict_map: BTreeMap::new(),
            file_system_policy_map: BTreeMap::new(),
            origin_agent_cluster_opt_ins_and_outs: BTreeMap::new(),
            origin_agent_cluster_states_by_browsing_instance: BTreeMap::new(),
        }
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
    /// `origin_agent_cluster_lock_`) to reduce thread contention, the
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
    pub(crate) fn get_locked_instance() -> MutexGuard<'static, ChildProcessSecurityPolicyImpl> {
        // `unwrap` is ok, because Chromium is always built with `-Cpanic=abort`
        // which means that a Mutex cannot be poisoned when unwinding a panic
        // while holding the mutex.
        //
        // TODO(crbug.com/477584253): Consider switching this to use
        // std::sync::nonpoison::Mutex once it is stabilized.
        Self::get_instance().lock().unwrap()
    }
}

/// A unique identifier for a `BrowserContext`. Currently, this is based on the
/// string representation of the C++ `BrowserContext::UniqueToken()`.
// TODO(crbug.com/522298905): Add FFI for UnguessableToken so that
// `UniqueToken()` can be used by both Rust and C++.
#[derive(PartialEq, Eq, PartialOrd, Ord, Clone)]
pub struct BrowserContextId(String);

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
