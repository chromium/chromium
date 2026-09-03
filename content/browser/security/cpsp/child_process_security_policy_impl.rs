// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cxx::UniquePtr;
use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::path::PathBuf;
use std::sync::{LazyLock, Mutex, MutexGuard};

use base_file_path::ffi::FilePath;
use content_browser_id_types::BrowsingInstanceId;
use storage_common::FileSystemType;
use unguessable_token::UnguessableToken;
use url::Origin;

use crate::process_state::ProcessStateMaps;

/// This block defines the Foreign Function Interface for C++ code to call the
/// specified Rust functions. The functions operate on a
/// ChildProcessSecurityPolicyImpl singleton defined further below.
#[cxx::bridge(namespace = "content::rust::child_process_security_policy")]
mod ffi {
    #![allow(unsafe_code)]
    unsafe extern "C++" {
        include!("base/files/file_path.rs.h");
        include!("base/unguessable_token.h");
        include!("url/origin.rs.h");
        include!("content/browser/isolated_origin_util.h");
        include!("storage/common/file_system/file_system_types.h");
        include!("content/public/browser/browsing_instance_id.h");
        include!("content/public/common/child_process_id.h");
        include!("content/browser/security/cpsp/child_process_security_policy_impl.h");
        include!("content/browser/security/cpsp/child_process_security_policy_shim.h");

        #[namespace = "base"]
        type FilePath = base_file_path::ffi::FilePath;

        #[namespace = "base"]
        type UnguessableToken = unguessable_token::UnguessableToken;

        // Gives us access to C++ url::Origin and all methods exposed in the origin
        // bridge file without having to redefine them here.
        #[namespace = "url"]
        type Origin = url::origin::ffi::Origin;
        #[namespace = ""]
        type GURL = url::gurl::ffi::GURL;

        #[namespace = "content"]
        type IsolatedOriginUtil;

        #[namespace = "content::ChildProcessSecurityPolicy"]
        type IsolatedOriginSource = super::IsolatedOriginSource;

        #[namespace = "content"]
        #[Self = "IsolatedOriginUtil"]
        #[cxx_name = "IsValidIsolatedOrigin"]
        fn is_valid_isolated_origin(origin: &Origin) -> bool;

        #[namespace = "content"]
        #[Self = "IsolatedOriginUtil"]
        #[cxx_name = "DoesOriginMatchIsolatedOrigin"]
        fn does_origin_match_isolated_origin(origin: &Origin, isolated_origin: &Origin) -> bool;

        #[namespace = "content"]
        #[Self = "IsolatedOriginUtil"]
        #[cxx_name = "IsValidOriginForOriginAgentClusterOptIn"]
        fn is_valid_origin_for_origin_agent_cluster_opt_in(origin: &Origin) -> bool;

        #[namespace = "content"]
        #[Self = "IsolatedOriginUtil"]
        #[cxx_name = "IsValidOriginForOriginAgentClusterOptOut"]
        fn is_valid_origin_for_origin_agent_cluster_opt_out(origin: &Origin) -> bool;

        #[namespace = "content::rust::child_process_security_policy"]
        #[cxx_name = "PushOriginToVector"]
        fn push_origin_to_vector(origin: &Origin, vec: Pin<&mut CxxVector<Origin>>);

        #[namespace = "content::rust::child_process_security_policy"]
        #[cxx_name = "GetSiteForOrigin"]
        fn get_site_for_origin(origin: &Origin) -> UniquePtr<GURL>;

        #[namespace = "content::rust::child_process_security_policy"]
        #[cxx_name = "RemoveTrailingDotFromUrlIfNecessary"]
        fn remove_trailing_dot_from_url_if_necessary(site_url: &GURL) -> UniquePtr<GURL>;

        #[namespace = "content::rust::child_process_security_policy"]
        #[cxx_name = "CreateOriginWithDefaultPortIfNecessary"]
        fn create_origin_with_default_port_if_necessary(origin: &Origin) -> UniquePtr<Origin>;

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
        fn remove_v8_optimization_state(browsing_instance_id: BrowsingInstanceId);

        fn register_file_system_permission_policy(file_system_type: FileSystemType, policy: i32);
        fn find_permissions_for_file_system_type(
            file_system_type: FileSystemType,
            policy: &mut i32,
        ) -> bool;

        fn record_origin_agent_cluster_request_if_new(
            browser_context_token: UnguessableToken,
            origin: UniquePtr<Origin>,
        ) -> bool;
        fn has_origin_ever_requested_origin_agent_cluster_value(
            browser_context_token: UnguessableToken,
            origin: UniquePtr<Origin>,
        ) -> bool;
        fn remove_origin_agent_cluster_requests_for_browser_context(
            browser_context_token: UnguessableToken,
        );

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
            browser_context_token: UnguessableToken,
            origin: UniquePtr<Origin>,
            oac_state: OriginAgentClusterIsolationState,
            is_global_walk_or_frame_removal: bool,
        );
        fn remove_origin_agent_cluster_state(browsing_instance_id: BrowsingInstanceId);

        fn grant_file_for_browser_upload(owner_token: UnguessableToken, file: &FilePath);
        fn revoke_file_for_browser_upload(owner_token: UnguessableToken);
        fn can_read_file_for_browser_upload(file: &FilePath) -> bool;

        fn add_isolated_origin_internal(
            browser_context_id: UnguessableToken,
            origin_to_add: UniquePtr<Origin>,
            applies_to_future_browsing_instances: bool,
            browsing_instance_id: BrowsingInstanceId,
            isolate_all_subdomains: bool,
            source: IsolatedOriginSource,
        );
        fn get_isolated_origins(
            has_source: bool,
            source: IsolatedOriginSource,
            browser_context_id: UnguessableToken,
            origins: Pin<&mut CxxVector<Origin>>,
        );
        fn get_matching_process_isolated_origin_from_legacy_origin_list(
            browser_context_id: UnguessableToken,
            browsing_instance_id: BrowsingInstanceId,
            origin: UniquePtr<Origin>,
            site_url: UniquePtr<GURL>,
        ) -> UniquePtr<Origin>;
        fn is_isolated_site_from_source(
            origin: UniquePtr<Origin>,
            source: IsolatedOriginSource,
        ) -> bool;
        fn get_isolated_origin_entry_count_for_testing(origin: UniquePtr<Origin>) -> i32;
        fn remove_isolated_origins_for_browser_context(browser_context_id: UnguessableToken);
        fn remove_isolated_origins_for_browsing_instance(browsing_instance_id: BrowsingInstanceId);
        fn remove_isolated_origin_for_testing(origin: UniquePtr<Origin>);
        fn clear_isolated_origins_for_testing();
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

fn remove_v8_optimization_state(browsing_instance_id: BrowsingInstanceId) {
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
    browser_context_token: UnguessableToken,
    origin: UniquePtr<ffi::Origin>,
) -> bool {
    if !ffi::IsolatedOriginUtil::is_valid_origin_for_origin_agent_cluster_opt_in(&origin) {
        return false;
    }

    let browser_context_id = BrowserContextId(browser_context_token);
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    let origins = cpsp.origin_agent_cluster_opt_ins_and_outs.entry(browser_context_id).or_default();

    if origins.contains(&origin) {
        return false;
    }

    origins.insert(origin);
    true
}

fn has_origin_ever_requested_origin_agent_cluster_value(
    browser_context_token: UnguessableToken,
    origin: UniquePtr<ffi::Origin>,
) -> bool {
    let browser_context_id = BrowserContextId(browser_context_token);
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.origin_agent_cluster_opt_ins_and_outs
        .get(&browser_context_id)
        .is_some_and(|origins| origins.contains(&origin))
}

fn remove_origin_agent_cluster_requests_for_browser_context(
    browser_context_token: UnguessableToken,
) {
    let browser_context_id = BrowserContextId(browser_context_token);
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
    browser_context_token: UnguessableToken,
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
        let browser_context_id = BrowserContextId(browser_context_token);
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

fn remove_origin_agent_cluster_state(browsing_instance_id: BrowsingInstanceId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.origin_agent_cluster_states_by_browsing_instance.remove(&browsing_instance_id);
}

fn grant_file_for_browser_upload(owner_token: UnguessableToken, file: &FilePath) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.browser_granted_files.entry(file.to_path_buf()).or_default().push(owner_token);
}

fn revoke_file_for_browser_upload(owner_token: UnguessableToken) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.browser_granted_files.retain(|_, tokens| {
        tokens.retain(|token| *token != owner_token);
        !tokens.is_empty()
    });
}

fn can_read_file_for_browser_upload(file: &FilePath) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    #[cfg(unix)]
    {
        // On Unix, `as_path()` provides a zero-copy `&Path` reference to the
        // underlying bytes in `FilePath`, avoiding a heap allocation.
        cpsp.browser_granted_files.contains_key(file.as_path())
    }
    #[cfg(windows)]
    {
        // Windows paths require converting UTF-16 to WTF-8, which allocates a
        // `PathBuf`.
        cpsp.browser_granted_files.contains_key(&file.to_path_buf())
    }
}

fn add_isolated_origin_internal(
    browser_context_id: UnguessableToken,
    origin_to_add: UniquePtr<ffi::Origin>,
    applies_to_future_browsing_instances: bool,
    browsing_instance_id: BrowsingInstanceId,
    isolate_all_subdomains: bool,
    source: IsolatedOriginSource,
) {
    // GetSiteForOrigin() is used to look up the site URL of `origin_to_add` to
    // speed up the isolated origin lookup.  This only performs a
    // straightforward translation of an origin to eTLD+1; it does *not* take
    // into account effective URLs, isolated origins, and other logic that's not
    // needed here, but *is* typically needed for making process model
    // decisions. Be very careful about using GetSiteForOrigin() elsewhere, and
    // consider whether you should be using SiteInfo::Create() instead.
    let site_url = ffi::get_site_for_origin(&origin_to_add);

    let browser_context_id = if browser_context_id.is_empty() {
        None
    } else {
        Some(BrowserContextId(browser_context_id))
    };
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    let entries = cpsp.isolated_origins.entry(site_url).or_default();

    // Check if the origin to be added already exists, in which case it may not
    // need to be added again.
    let mut should_add = true;
    for entry in entries.iter() {
        // TODO(crbug.com/40171707): The exact origin comparison here allows
        // redundant entries with certain uses of `isolate_all_subdomains`.
        if *entry.origin != *origin_to_add {
            continue;
        }

        // If the added origin already exists for the same BrowserContext and
        // covers the same BrowsingInstances, don't re-add it.
        if entry.browser_context_id == browser_context_id
            && entry.matches_browsing_instance(browsing_instance_id)
        {
            if entry.applies_to_future_browsing_instances {
                if applies_to_future_browsing_instances {
                    // If the existing entry applies to future
                    // BrowsingInstances, and the new isolated origin is also
                    // requested to apply to future BrowsingInstances, the
                    // threshold ID must necessarily be greater than the old ID,
                    // since NextBrowsingInstanceId() returns monotonically
                    // increasing IDs.
                    assert!(entry.browsing_instance_id <= browsing_instance_id);
                }
            } else {
                // If an origin had been added for a specific BrowsingInstance,
                // we can't later receive a request to isolate that origin
                // within future BrowsingInstances that start at the same (or
                // lower) BrowsingInstance. Requests to isolate future
                // BrowsingInstances should always reference
                // SiteInstanceImpl::NextBrowsingInstanceId(), which always
                // refers to an ID that's greater than any existing
                // BrowsingInstance ID.
                assert!(!applies_to_future_browsing_instances);
            }
            should_add = false;
            break;
        }

        // Otherwise, allow the origin to be added again for a different profile
        // (or globally for all profiles), possibly with a different
        // BrowsingInstance ID cutoff.  Note that a particular origin might have
        // multiple entries, each one for a different profile, so we must loop
        // over all such existing entries before concluding that `origin` really
        // needs to be added.
    }

    if should_add {
        entries.push(IsolatedOriginEntry {
            origin: origin_to_add,
            applies_to_future_browsing_instances,
            browsing_instance_id,
            browser_context_id,
            isolate_all_subdomains,
            source,
        });
    }
}

// TODO(crbug.com/482216433): Pass `source` as an optional type and remove
// `has_source` once the CXX has optional support (or by using Crubit). See:
// https://github.com/dtolnay/cxx/issues/87
//
// Note on `origins`: Cxx does not support returning or passing C++ vectors of
// opaque types by value. Instead, we pass a mutable reference to a C++ vector
// using `Pin<&mut CxxVector<T>>`. The `Pin` guarantees that the C++ object will
// not be moved in memory by Rust. Elements are appended to it via the
// `push_origin_to_vector` FFI shim.
fn get_isolated_origins(
    has_source: bool,
    source: IsolatedOriginSource,
    browser_context_id: UnguessableToken,
    mut origins: std::pin::Pin<&mut cxx::CxxVector<ffi::Origin>>,
) {
    let browser_context_id = BrowserContextId(browser_context_id);
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();

    for entries in cpsp.isolated_origins.values() {
        for entry in entries {
            if has_source && source != entry.source {
                continue;
            }

            if !entry.matches_profile(&browser_context_id) {
                continue;
            }

            // Do not include origins that only apply to specific
            // BrowsingInstances for this API.
            if !entry.applies_to_future_browsing_instances {
                continue;
            }

            ffi::push_origin_to_vector(&entry.origin, origins.as_mut());
        }
    }
}

fn get_matching_process_isolated_origin_from_legacy_origin_list(
    browser_context_id: UnguessableToken,
    browsing_instance_id: BrowsingInstanceId,
    origin: cxx::UniquePtr<ffi::Origin>,
    site_url: cxx::UniquePtr<ffi::GURL>,
) -> cxx::UniquePtr<ffi::Origin> {
    let browser_context_id = BrowserContextId(browser_context_id);
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();

    let mut best_match: Option<&cxx::UniquePtr<ffi::Origin>> = None;

    let mut entries_opt = cpsp.isolated_origins.get(&site_url);

    // Subtle corner case: if the site's host ends with a dot, do the lookup
    // without it.  A trailing dot shouldn't be able to bypass isolated origins:
    // if "https://foo.com" is an isolated origin, "https://foo.com." should
    // match it. For now, this trailing dot check and removal is done on the C++
    // side; it can move to Rust once there's better support for something
    // similar to GURL::Replacements.
    if entries_opt.is_none() {
        let fallback_site_url = ffi::remove_trailing_dot_from_url_if_necessary(&site_url);
        if !fallback_site_url.is_null() {
            entries_opt = cpsp.isolated_origins.get(&fallback_site_url);
        }
    }

    // Looks for all isolated origins that were already isolated at the time the
    // BrowsingInstance corresponding to `browsing_instance_id` was created. If
    // multiple isolated origins are registered with a common domain suffix,
    // return the most specific one.  For example, if foo.isolated.com and
    // isolated.com are both isolated origins, bar.foo.isolated.com should
    // return foo.isolated.com.
    if let Some(entries) = entries_opt {
        for entry in entries {
            // If this isolated origin applies only to a specific profile, don't
            // use it for a different profile.
            if !entry.matches_profile(&browser_context_id) {
                continue;
            }

            if entry.matches_browsing_instance(browsing_instance_id)
                && ffi::IsolatedOriginUtil::does_origin_match_isolated_origin(
                    &origin,
                    &entry.origin,
                )
            {
                // If a match has been found that requires all subdomains to be
                // isolated then return immediately. `origin` is returned to
                // ensure proper process isolation, e.g.
                // https://a.b.c.isolated.com matches an IsolatedOriginEntry
                // constructed from http://[*.]isolated.com, so
                // https://a.b.c.isolated.com must be returned.
                if entry.isolate_all_subdomains {
                    return ffi::create_origin_with_default_port_if_necessary(&origin);
                }

                if best_match.is_none()
                    || best_match.as_ref().unwrap().host().len() < entry.origin.host().len()
                {
                    best_match = Some(&entry.origin);
                }
            }
        }
    }

    if let Some(best) = best_match {
        return url::origin::ffi::clone_origin(best);
    } else {
        return cxx::UniquePtr::null();
    }
}

fn remove_isolated_origins_for_browser_context(browser_context_id: UnguessableToken) {
    let browser_context_id = Some(BrowserContextId(browser_context_id));
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();

    for entries in cpsp.isolated_origins.values_mut() {
        entries.retain(|entry| entry.browser_context_id != browser_context_id);
    }

    cpsp.isolated_origins.retain(|_, entries| !entries.is_empty());
}

fn remove_isolated_origins_for_browsing_instance(browsing_instance_id: BrowsingInstanceId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();

    for entries in cpsp.isolated_origins.values_mut() {
        // Remove entries that are specific to `browsing_instance_id` and
        // do not apply to future BrowsingInstances.
        entries.retain(|entry| {
            !(entry.browsing_instance_id == browsing_instance_id
                && !entry.applies_to_future_browsing_instances)
        });
    }

    // Also remove map entries for site URLs which no longer have any
    // IsolatedOriginEntries remaining.
    cpsp.isolated_origins.retain(|_, entries| !entries.is_empty());
}

fn is_isolated_site_from_source(
    origin: cxx::UniquePtr<ffi::Origin>,
    source: IsolatedOriginSource,
) -> bool {
    // Determine whether the scheme+eTLD+1 (the site URL) of `origin` is already
    // isolated due to `source`. Because COOP-triggered isolation isolates the
    // entire site (eTLD+1) rather than just the specific origin, we look up
    // using the `site_url` key, and then verify if the entry's origin is
    // exactly equal to the site's origin. This function assumes that the
    // passed-in `origin` is a valid non-opaque origin, which is currently
    // guaranteed in the callers by checking
    // `IsolatedOriginUtil::IsValidIsolatedOrigin(origin)` prior to calling
    // this.
    //
    // TODO(crbug.com/482216433): Consider passing in `site_url` as a site URL
    // type that can be precomputed by GetSiteForOrigin() to avoid making a copy
    // of origin just to call back into C++ to convert it to a site URL.
    let site_url = ffi::get_site_for_origin(&origin);
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(entries) = cpsp.isolated_origins.get(&site_url) {
        let site_origin = url::origin::ffi::create_origin_from_gurl(&site_url);
        for entry in entries {
            if entry.source == source && *entry.origin == *site_origin {
                return true;
            }
        }
    }
    false
}

fn get_isolated_origin_entry_count_for_testing(origin: cxx::UniquePtr<ffi::Origin>) -> i32 {
    let site_url = ffi::get_site_for_origin(&origin);
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(entries) = cpsp.isolated_origins.get(&site_url) {
        entries
            .iter()
            .filter(|entry| {
                // `*entry.origin == *origin` performs a deep comparison of the
                // underlying C++ `url::Origin` objects.
                *entry.origin == *origin
            })
            .count() as i32
    } else {
        0
    }
}

fn remove_isolated_origin_for_testing(origin: cxx::UniquePtr<ffi::Origin>) {
    let site_url = ffi::get_site_for_origin(&origin);
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();

    if let Some(entries) = cpsp.isolated_origins.get_mut(&site_url) {
        entries.retain(|entry| *entry.origin != *origin);
        if entries.is_empty() {
            cpsp.isolated_origins.remove(&site_url);
        }
    }
}

fn clear_isolated_origins_for_testing() {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.isolated_origins.clear();
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
    file_system_policy_map: HashMap<FileSystemType, i32>,

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

    /// Tracks files that the browser process has granted permission to the
    /// network service to upload on the user's behalf.
    ///
    /// Each file path maps to a list of tokens representing the active requests
    /// that have been granted access to this file. A token is added when a
    /// request is created, and it is removed from all associated file paths
    /// when the request is destroyed. Access is allowed as long as the file is
    /// present in this map.
    ///
    /// `PathBuf` is used as the key instead of `base::FilePath` as it is Rust's
    /// native path type, providing standard hashing and equality and avoiding
    /// heap-allocating a `cxx::UniquePtr` wrapper for each entry. It also
    /// enables a zero-copy `&Path` lookup optimization in
    /// `can_read_file_for_browser_upload` on Unix platforms.
    browser_granted_files: HashMap<PathBuf, Vec<UnguessableToken>>,

    // Tracks origins for which the entire origin should be treated as a site
    // when making process model decisions, rather than the origin's scheme and
    // eTLD+1. Each of these origins requires a dedicated process.
    //
    // The origins are stored in a map indexed by a site URL computed for each
    // origin.  For example, adding https://foo.com, https://bar.foo.com, and
    // https://www.bar.com would result in the following structure:
    //
    //   https://foo.com -> { https://foo.com, https://bar.foo.com }
    //   https://bar.com -> { https://www.bar.com }
    //
    // This organization speeds up lookups of isolated origins. The site can be
    // found in O(log n) time, and the corresponding list of origins to search
    // using the expensive DoesOriginMatchIsolatedOrigin() comparison is
    // typically small.
    //
    // Each origin entry stores information about:
    //
    // 1. Which BrowsingInstances it applies to. This is a combination of a
    //    BrowsingInstance ID `browsing_instance_id` and a bool flag
    //    `applies_to_future_browsing_instances` stored in in each origin's
    //    IsolatedOriginEntry. When `applies_to_future_browsing_instances` is
    //    true, the origin will be isolated in all BrowsingInstances with IDs
    //    equal to or greater than `browsing_instance_id`. When
    //    `applies_to_future_browsing_instances` is false, the origin will be
    //    isolated only in a single BrowsingInstance with ID
    //    `browsing_instance_id`.
    //
    // 2. Optionally, which BrowserContext (profile) it applies to.  When the
    //    `browser_context_id` field in the IsolatedOriginEntry is not None, a
    //    particular isolated origin entry only applies to that BrowserContext.
    //    Note that the same origin may be isolated in different profiles,
    //    possibly with different BrowsingInstance ID cut-offs.  For example:
    //
    //        https://foo.com -> { [https://test.foo.com profile_1 4],
    //                             [https://test.foo.com profile_2 7] }
    //
    //    represents https://test.foo.com being isolated in profile_1 with
    //    BrowsingInstance ID 4, and also in profile_2 with BrowsingInstance
    //    ID 7.
    //
    // TODO(crbug.com/482216433): Consider defining and using a SiteUrl or
    // PrincipalUrl type instead of GURL for the key of this map.
    isolated_origins: BTreeMap<cxx::UniquePtr<ffi::GURL>, Vec<IsolatedOriginEntry>>,
}

// This struct holds an isolated origin along with information such as which
// BrowsingInstances and profile it applies to.  See the `isolated_origins`
// field in ChildProcessSecurityPolicyImpl above for more details.
struct IsolatedOriginEntry {
    // The origin to be isolated.
    origin: cxx::UniquePtr<ffi::Origin>,

    // If this is false, the origin is isolated only in the BrowsingInstance
    // specified by `browsing_instance_id`.  If this is true, the origin is
    // isolated in all BrowsingInstances that have an ID equal to or
    // greater than `browsing_instance_id`.
    applies_to_future_browsing_instances: bool,

    // Specifies which BrowsingInstance(s) this IsolatedOriginEntry applies to.
    // When `applies_to_future_browsing_instances` is false, this refers to a
    // specific BrowsingInstance.  Otherwise, it specifies the minimum
    // BrowsingInstance ID, and the origin is isolated in all BrowsingInstances
    // with IDs greater than or equal to this value.
    browsing_instance_id: BrowsingInstanceId,

    // Optional information about the profile where the isolated origin applies.
    // If this is None, then the isolated origin applies globally to all
    // profiles.
    browser_context_id: Option<BrowserContextId>,

    // True if origins at this or lower level should be treated as distinct
    // isolated origins, effectively isolating all domains below a given domain,
    // e.g. if the origin is https://foo.com and `isolate_all_subdomains` is
    // true, then https://bar.foo.com, https://qux.bar.foo.com and all
    // subdomains of the form https://<<any pattern here>>.foo.com are
    // considered isolated origins.
    isolate_all_subdomains: bool,

    // This tracks the source of each isolated origin entry, e.g., to
    // distinguish those that should be displayed to the user from those that
    // should not. See https://crbug.com/40609024.
    source: IsolatedOriginSource,
}

impl IsolatedOriginEntry {
    // True if (1) this entry applies to all profiles, or (2) this entry is
    // associated with the same profile as `requested_browser_context_id`.
    fn matches_profile(&self, requested_browser_context_id: &BrowserContextId) -> bool {
        match self.browser_context_id {
            None => true,
            Some(entry_id) => {
                !requested_browser_context_id.0.is_empty()
                    && entry_id == *requested_browser_context_id
            }
        }
    }

    // True if this entry applies to the BrowsingInstance specified by
    // `browsing_instance_id`.  See `applies_to_future_browsing_instances` and
    // `browsing_instance_id` declarations in IsolatedOriginEntry for more
    // details.
    fn matches_browsing_instance(&self, browsing_instance_id: BrowsingInstanceId) -> bool {
        if self.applies_to_future_browsing_instances {
            self.browsing_instance_id <= browsing_instance_id
        } else {
            self.browsing_instance_id == browsing_instance_id
        }
    }
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
            file_system_policy_map: HashMap::new(),
            origin_agent_cluster_opt_ins_and_outs: BTreeMap::new(),
            origin_agent_cluster_states_by_browsing_instance: BTreeMap::new(),
            browser_granted_files: HashMap::new(),
            isolated_origins: BTreeMap::new(),
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

/// A unique identifier for a `BrowserContext`, wrapping C++
/// `base::UnguessableToken` returned by `BrowserContext::UniqueToken()`.
#[derive(PartialEq, Eq, PartialOrd, Ord, Copy, Clone, Debug, Hash)]
pub struct BrowserContextId(pub UnguessableToken);

/// Rust equivalent of the C++ ChildProcessSecurityPolicy::IsolatedOriginSource
/// enum. Using cxx::ExternType with cxx::kind::Trivial ensures that this enum
/// can be passed over from C++ to Rust with zero cost. [allow(dead_code)] is
/// necessary because the individual values are currently only created and used
/// on the C++ side but not yet on the Rust side.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
#[allow(dead_code)]
// LINT.IfChange(IsolatedOriginSource)
enum IsolatedOriginSource {
    BuiltIn,
    CommandLine,
    FieldTrial,
    Policy,
    UserTriggered,
    WebTriggered,
    Test,
}
// LINT.ThenChange(//content/public/browser/child_process_security_policy.h:IsolatedOriginSource)

#[allow(unsafe_code)]
// SAFETY: IsolatedOriginSource is a simple enum with a fixed repr(i32) that
// matches the C++ enum's underlying type and values.
unsafe impl cxx::ExternType for IsolatedOriginSource {
    type Id = cxx::type_id!("content::ChildProcessSecurityPolicy::IsolatedOriginSource");
    type Kind = cxx::kind::Trivial;
}

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
