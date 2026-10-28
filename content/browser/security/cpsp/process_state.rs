// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:feature";
    "//content/common:common_id_types";
    "//url:url_rust";
}

use std::collections::hash_map::Entry;
use std::collections::{BTreeMap, BTreeSet, HashMap};

use common_id_types::ChildProcessId;
use cxx::UniquePtr;

use crate::process_lock::{AssignableProcessLock, StatefulProcessLock};
use crate::ChildProcessSecurityPolicyImpl;

#[cxx::bridge(namespace = "content::rust::child_process_security_policy")]
pub(crate) mod ffi {
    #![allow(unsafe_code)]
    unsafe extern "C++" {
        include!("content/public/common/child_process_id.h");
        include!("content/browser/process_lock.h");
        include!("content/browser/process_lock_shim.h");
        include!("content/browser/security/cpsp/child_process_security_policy_shim.h");
        include!("content/browser/site_instance_impl.h");
        include!("url/origin.rs.h");

        #[namespace = "content"]
        type ChildProcessId = super::common_id_types::ChildProcessId;

        #[namespace = ""]
        type GURL = super::url_rust::gurl::ffi::GURL;

        #[namespace = "url"]
        type Origin = super::url_rust::origin::ffi::Origin;

        #[namespace = "content"]
        #[rust_name = "CppProcessLock"]
        type ProcessLock;

        #[rust_name = "is_locked_to_site"]
        fn IsLockedToSite(self: &CppProcessLock) -> bool;
        #[rust_name = "allows_any_site"]
        fn AllowsAnySite(self: &CppProcessLock) -> bool;
        fn is_unused(self: &CppProcessLock) -> bool;

        #[namespace = "content"]
        type SiteInstanceImpl;

        #[namespace = "content"]
        #[rust_name = "get_default_site_url"]
        #[Self = "SiteInstanceImpl"]
        fn GetDefaultSiteURL() -> &'static GURL;

        // ProcessLock helpers in C++
        #[namespace = "content::rust::process_lock"]
        #[cxx_name = "GetProcessLockURL"]
        fn get_process_lock_url(lock: &CppProcessLock) -> UniquePtr<GURL>;
        #[namespace = "content::rust::process_lock"]
        #[cxx_name = "HaveSameWebExposedIsolationInfo"]
        fn have_same_web_exposed_isolation_info(a: &CppProcessLock, b: &CppProcessLock) -> bool;
        #[namespace = "content::rust::process_lock"]
        #[cxx_name = "ToString"]
        fn to_string(lock: &CppProcessLock) -> String;
        #[namespace = "content::rust::process_lock"]
        #[cxx_name = "CreateInvalid"]
        fn create_invalid() -> UniquePtr<CppProcessLock>;
        #[namespace = "content::rust::process_lock"]
        #[cxx_name = "Clone"]
        fn clone(lock: &CppProcessLock) -> UniquePtr<CppProcessLock>;
        #[namespace = "content::rust::process_lock"]
        #[cxx_name = "SetIsUsed"]
        fn set_is_used(lock: Pin<&mut CppProcessLock>);

        #[cfg(IS_CHROMEOS)]
        #[cxx_name = "NormalizeExternalFileUrl"]
        fn normalize_external_file_url(url: &GURL) -> UniquePtr<GURL>;
    }

    extern "Rust" {
        // Process state management.
        fn create_state_for_process(child_id: ChildProcessId);
        fn prepare_to_remove_state(child_id: ChildProcessId);
        fn complete_pending_state_removal(child_id: ChildProcessId);

        // Per-child process state methods.
        fn lock_process(child_id: ChildProcessId, process_lock: &CppProcessLock);
        fn set_process_is_used(child_id: ChildProcessId);
        fn get_process_lock(child_id: ChildProcessId) -> UniquePtr<CppProcessLock>;

        fn grant_commit_origin(child_id: ChildProcessId, origin: UniquePtr<Origin>);
        fn grant_request_origin(child_id: ChildProcessId, origin: UniquePtr<Origin>);

        fn grant_commit_scheme(child_id: ChildProcessId, scheme: &str);
        fn grant_request_scheme(child_id: ChildProcessId, scheme: &str);

        #[cfg(IS_CHROMEOS)]
        fn grant_request_of_external_file_url(child_id: ChildProcessId, url: &GURL);
        #[cfg(IS_CHROMEOS)]
        fn grant_commit_of_external_file_url(child_id: ChildProcessId, url: &GURL);

        fn grant_web_ui_bindings(child_id: ChildProcessId);
        fn has_web_ui_bindings(child_id: ChildProcessId) -> bool;

        fn grant_origin_check_exemption_for_webview(
            child_id: ChildProcessId,
            origin: UniquePtr<Origin>,
        );
        fn has_origin_check_exemption_for_webview(
            child_id: ChildProcessId,
            origin: UniquePtr<Origin>,
        ) -> bool;

        fn grant_send_midi_message(child_id: ChildProcessId);
        fn grant_send_midi_sysex_message(child_id: ChildProcessId);
        fn can_send_midi_message(child_id: ChildProcessId) -> bool;
        fn can_send_midi_sysex_message(child_id: ChildProcessId) -> bool;
    }
}

#[allow(unsafe_code)]
// TODO(crbug.com/343218479): On Windows component builds, `blink_common` is a
// separate DLL. Accessing its global variables from another component requires
// specifying the library name and dylib link kind for the linker to resolve
// the import symbol correctly. Remove this workaround once first-class Rust
// component support is established.
#[cfg_attr(
    all(target_os = "windows", component_build),
    link(name = "blink_common", kind = "dylib")
)]
unsafe extern "C" {
    pub static kBlockMidiByDefault: feature::Feature;
}

fn create_state_for_process(child_id: ChildProcessId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.process_states.create_state_for_process(child_id);
}

fn prepare_to_remove_state(child_id: ChildProcessId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.process_states.prepare_to_remove_state(child_id);
}

fn complete_pending_state_removal(child_id: ChildProcessId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.process_states.complete_pending_state_removal(child_id);
}

fn lock_process(child_id: ChildProcessId, process_lock: &ffi::CppProcessLock) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    let state = cpsp.process_states.get_mut(&child_id).expect("The process state should exist.");
    let cloned_lock = ffi::clone(process_lock);
    let lock_to_set = AssignableProcessLock::lift(cloned_lock);
    state.set_process_lock(lock_to_set);
}

fn set_process_is_used(child_id: ChildProcessId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    let state = cpsp.process_states.get_mut(&child_id).expect("The process state should exist.");
    state.set_process_is_used();
}

fn get_process_lock(child_id: ChildProcessId) -> UniquePtr<ffi::CppProcessLock> {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_for_query(&child_id) {
        ffi::clone(state.process_lock())
    } else {
        ffi::create_invalid()
    }
}

fn grant_commit_origin(child_id: ChildProcessId, origin: UniquePtr<ffi::Origin>) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_commit_origin(origin);
    }
}

fn grant_request_origin(child_id: ChildProcessId, origin: UniquePtr<ffi::Origin>) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_request_origin(origin);
    }
}

fn grant_commit_scheme(child_id: ChildProcessId, scheme: &str) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_commit_scheme(scheme);
    }
}

fn grant_request_scheme(child_id: ChildProcessId, scheme: &str) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_request_scheme(scheme);
    }
}

#[cfg(IS_CHROMEOS)]
fn grant_request_of_external_file_url(child_id: ChildProcessId, url: &ffi::GURL) {
    if !url.is_valid() {
        return;
    }

    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_request_of_external_file_url(url);
    }
}

#[cfg(IS_CHROMEOS)]
fn grant_commit_of_external_file_url(child_id: ChildProcessId, url: &ffi::GURL) {
    if !url.is_valid() {
        return;
    }

    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_commit_of_external_file_url(url);
    }
}

fn grant_web_ui_bindings(child_id: ChildProcessId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_web_ui_bindings();
    }
}

fn has_web_ui_bindings(child_id: ChildProcessId) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.process_states.get_for_query(&child_id).is_some_and(|state| state.has_web_ui_bindings())
}

fn grant_origin_check_exemption_for_webview(
    child_id: ChildProcessId,
    origin: UniquePtr<ffi::Origin>,
) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_origin_check_exemption_for_webview(origin);
    }
}

fn has_origin_check_exemption_for_webview(
    child_id: ChildProcessId,
    origin: UniquePtr<ffi::Origin>,
) -> bool {
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.process_states
        .get_for_query(&child_id)
        .is_some_and(|state| state.has_origin_check_exemption_for_webview(&origin))
}

#[allow(unsafe_code)]
fn grant_send_midi_message(child_id: ChildProcessId) {
    // SAFETY: `kBlockMidiByDefault` is defined in C++ via `BASE_FEATURE`
    // and is thread-safe to query.
    let block_midi_by_default = unsafe { kBlockMidiByDefault.is_enabled() };
    if !block_midi_by_default {
        return;
    }

    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_send_midi_message();
    }
}

fn grant_send_midi_sysex_message(child_id: ChildProcessId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    if let Some(state) = cpsp.process_states.get_mut(&child_id) {
        state.grant_send_midi_sysex_message();
    }
}

#[allow(unsafe_code)]
fn can_send_midi_message(child_id: ChildProcessId) -> bool {
    // SAFETY: `kBlockMidiByDefault` is defined in C++ via `BASE_FEATURE`
    // and is thread-safe to query.
    let block_midi_by_default = unsafe { kBlockMidiByDefault.is_enabled() };
    if !block_midi_by_default {
        return true;
    }

    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.process_states.get_for_query(&child_id).is_some_and(|state| state.can_send_midi_message())
}

fn can_send_midi_sysex_message(child_id: ChildProcessId) -> bool {
    // Note: The C++ version asserts that a process cannot have SysEx permission
    // without also having normal MIDI permission. In Rust, this invariant is
    // guaranteed by construction through the `MidiPermission` tri-state enum on
    // `ProcessState`, making a runtime check unnecessary here.
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.process_states
        .get_for_query(&child_id)
        .is_some_and(|state| state.can_send_midi_sysex_message())
}

/// Data structure that tracks ProcessState for each RenderProcessHost based on
/// ChildProcessId. A registered ProcessState is guaranteed to exist both while
/// the RenderProcessHost exists and until all of the
/// ChildProcessSecurityPolicy::Handles for the process have gone away.
///
/// The ProcessState can only be modified while the RenderProcessHost exists, so
/// that no new permissions can be granted after it is deleted. Queries for the
/// state can continue to be safely serviced until the Handles are gone.
///
/// This is enforced at compile time by putting all mutation APIs on the
/// MutableProcessState type wrapper, which only exists while the ProcessState
/// is tracked in the live_process_states map. Once the ProcessState is
/// transferred to the pending_remove_states map when the RenderProcessHost is
/// deleted, it can no longer be modified because the MutableProcessState
/// wrapper does not have a public constructor and is only created in the
/// create_state_for_process function. It also consumes the ProcessState, which
/// cannot happen while the state is owned by one of the maps.
///
/// All query functions must use `get_for_query` to look up ProcessState, which
/// is the only way to access the query APIs. This approach looks for the state
/// in both maps and has no mutator APIs.
///
/// All mutator functions must use `get_mut` to look up ProcessState, which is
/// the only way to access the mutator APIs. This approach only looks for the
/// state in `live_process_states` and has no query APIs.
pub(crate) struct ProcessStateMaps {
    /// ProcessStates registered by ChildProcessId, while the RenderProcessHost
    /// with that ChildProcessId still exists. ProcessStates in this map can be
    /// both queried and modified, and are guaranteed to exist in this map
    /// while the RenderProcessHost exists.
    live_process_states: HashMap<ChildProcessId, MutableProcessState>,

    /// ProcessStates registered by ChildProcessId, after the RenderProcessHost
    /// with that ChildProcessId has been deleted and until all corresponding
    /// ChildProcessSecurityPolicy::Handles have been deleted. This allows
    /// queries to succeed on other threads until they learn about the
    /// process's destruction, without allowing modifications to the state.
    pending_remove_states: HashMap<ChildProcessId, ProcessState>,
}

impl ProcessStateMaps {
    pub(crate) fn new() -> Self {
        Self { live_process_states: HashMap::new(), pending_remove_states: HashMap::new() }
    }

    /// Registers a new MutableProcessState for a new RenderProcessHost
    /// identified by `child_id`. Panics if this ID has already been
    /// registered or is pending removal.
    pub(crate) fn create_state_for_process(&mut self, child_id: ChildProcessId) {
        if self.pending_remove_states.contains_key(&child_id) {
            panic!("Process {child_id:?} is already pending removal.");
        }
        match self.live_process_states.entry(child_id) {
            Entry::Occupied(_) => {
                panic!("Process {child_id:?} has already been registered.");
            }
            Entry::Vacant(entry) => {
                entry.insert(MutableProcessState(ProcessState::new()));
            }
        }
    }

    /// When the RenderProcessHost with `child_id` is deleted, this function
    /// transitions the MutableProcessState to an immutable ProcessState in
    /// `pending_remove_states`, which continues to be used for queries until
    /// all Handles have been deleted. Panics if this ID was not already
    /// registered.
    pub(crate) fn prepare_to_remove_state(&mut self, child_id: ChildProcessId) {
        match self.live_process_states.entry(child_id) {
            Entry::Occupied(live_entry) => match self.pending_remove_states.entry(child_id) {
                Entry::Occupied(_) => {
                    panic!("Process {child_id:?} is already pending removal.");
                }
                Entry::Vacant(pending_remove_entry) => {
                    // Move the ProcessState (without its MutableProcessState
                    // wrapper) to the pending_remove_states map.
                    let mutable_state: MutableProcessState = live_entry.remove();
                    pending_remove_entry.insert(mutable_state.into_immutable());
                }
            },
            Entry::Vacant(_) => {
                panic!(
                    "Preparing to remove a process {child_id:?} that was not previously registered."
                );
            }
        }
    }

    /// When all Handles for `child_id` have been deleted, this function removes
    /// its ProcessState from ProcessStateMaps entirely. This assumes
    /// `prepare_to_remove_state` has already been called for this ID, and
    /// panics otherwise.
    pub(crate) fn complete_pending_state_removal(&mut self, child_id: ChildProcessId) {
        if self.live_process_states.contains_key(&child_id) {
            panic!("Process {child_id:?} has not yet gone through prepare_to_remove_state.");
        }
        match self.pending_remove_states.entry(child_id) {
            Entry::Occupied(entry) => {
                entry.remove();
            }
            Entry::Vacant(_) => {
                panic!("Removing a process {child_id:?} that is not pending removal.");
            }
        }
    }

    /// Returns a ProcessState for `child_id` for use by query functions, which
    /// should continue to work after the RenderProcessHost is gone as long as
    /// some Handles still exist.
    // TODO(crbug.com/476409377): Take proof that the process is still
    // registered (e.g., a RenderProcessHost, Handle, or some form of consumable
    // receipt) instead of a `child_id`, so that it is not possible to call this
    // API when no ProcessState exists. At that point, the return type does not
    // need to be `Option`. Same for `get_mut` below.
    pub(crate) fn get_for_query(&self, child_id: &ChildProcessId) -> Option<&ProcessState> {
        // Look in both `live_process_states` and `pending_remove_states`,
        // since queries should continue to work after the process is gone
        // until all Handles are gone as well.
        if let Some(live_state) = self.live_process_states.get(child_id) {
            // Return the internal ProcessState without the mutation APIs.
            return Some(live_state.as_immutable_state());
        }

        // TODO(crbug.com/522872468): The C++ version manually tracks reference
        // counts for Handles and ensures that this can only return a value
        // while they exist, or only on the IO thread after they are gone (while
        // a task is posted to that thread). The Rust code should manage the ref
        // counts.
        self.pending_remove_states.get(child_id)
    }

    /// Returns a MutableProcessState for `child_id` for use by mutator
    /// functions, which should only work while the RenderProcessHost for the ID
    /// exists.
    pub(crate) fn get_mut(
        &mut self,
        child_id: &ChildProcessId,
    ) -> Option<&mut MutableProcessState> {
        // Only look in `live_process_states`, since mutations are not allowed
        // after the process is gone.
        self.live_process_states.get_mut(child_id)
    }
}

/// Holds security state specific to each child process. This base type only
/// exposes query functions which work while the state is in either
/// ProcessStateMaps' `live_process_states` or `pending_remove_states`.
/// All mutator APIs exist in the MutableProcessState wrapper, which only
/// allows mutations while the state is in `live_process_states`.
///
/// ProcessState is guaranteed to exist and be registered in ProcessStateMaps
/// while either the corresponding RenderProcessHost or any of its
/// ChildProcessSecurityPolicy::Handles exist.
#[derive(Debug)]
pub(crate) struct ProcessState {
    /// The typestate-protected ProcessLock for this process. This determines
    /// which documents are allowed to load in a process and which site data the
    /// process is allowed to access, based on the SiteInfo principal.
    process_lock: StatefulProcessLock,
    /// Maps URL schemes to commit/request policies the child process has been
    /// granted. There is no provision for revoking.
    scheme_map: HashMap<String, CommitRequestPolicy>,
    /// The map of URL origins to commit/request policies the child process has
    /// been granted. There is no provision for revoking.
    ///
    /// Stored as a BTreeMap rather than a HashMap (which the C++ version uses)
    /// to avoid dealing with hash functions for url::Origin. The ordering is
    /// based on the C++ url::Origin comparison operators.
    origin_map: BTreeMap<UniquePtr<ffi::Origin>, CommitRequestPolicy>,
    /// The set of specific externalfile: URLs the child process is permitted to
    /// request or commit.
    ///
    /// Stored as a BTreeSet rather than a HashSet (which the C++ version uses)
    /// to avoid dealing with hash functions for GURL. The ordering and equality
    /// are based on GURL::possibly_invalid_spec(), matching the C++ GURL
    /// comparison operators.
    #[cfg(IS_CHROMEOS)]
    request_externalfile_set: BTreeSet<UniquePtr<ffi::GURL>>,
    #[cfg(IS_CHROMEOS)]
    commit_externalfile_set: BTreeSet<UniquePtr<ffi::GURL>>,
    /// Whether a child process can host WebUI pages with bindings.
    has_web_ui_bindings: bool,
    /// The set of origins in Android WebView and <webview> tags that are
    /// allowed to bypass some navigation checks. Limited to opaque origins
    /// loaded with LoadDataWithBaseURL and file origins loaded with
    /// allow_universal_access_from_file_urls.
    ///
    /// Stored as a BTreeSet rather than a HashSet (which the C++ version uses)
    /// to avoid dealing with hash functions for url::Origin. The ordering is
    /// based on the C++ url::Origin comparison operators.
    webview_origin_exemption_set: BTreeSet<UniquePtr<ffi::Origin>>,
    /// Determines if a child process can send MIDI messages.
    midi_permission: MidiPermission,
}

impl ProcessState {
    /// Private to the module to prevent creating new ProcessStates outside of
    /// ProcessStateMaps.
    fn new() -> Self {
        ProcessState {
            process_lock: StatefulProcessLock::default(),
            scheme_map: HashMap::new(),
            origin_map: BTreeMap::new(),
            #[cfg(IS_CHROMEOS)]
            request_externalfile_set: BTreeSet::new(),
            #[cfg(IS_CHROMEOS)]
            commit_externalfile_set: BTreeSet::new(),
            has_web_ui_bindings: false,
            webview_origin_exemption_set: BTreeSet::new(),
            midi_permission: MidiPermission::CannotSendMidi,
        }
    }

    fn process_lock(&self) -> &ffi::CppProcessLock {
        self.process_lock.as_ffi()
    }

    fn can_commit_origin(&self, origin: &UniquePtr<ffi::Origin>) -> bool {
        self.origin_map.get(origin) == Some(&CommitRequestPolicy::CommitAndRequest)
    }

    // TODO(crbug.com/522872468): Call from `ProcessState::can_request_url` once
    // ported to Rust.
    #[allow(dead_code)]
    fn can_request_origin(&self, origin: &UniquePtr<ffi::Origin>) -> bool {
        // Anything in `origin_map` must have at least request permissions.
        self.origin_map.contains_key(origin)
    }

    fn can_commit_scheme(&self, scheme: &str) -> bool {
        self.scheme_map.get(scheme) == Some(&CommitRequestPolicy::CommitAndRequest)
    }

    // TODO(crbug.com/522872468): Call from `ProcessState::can_request_url` once
    // ported to Rust.
    #[allow(dead_code)]
    fn can_request_scheme(&self, scheme: &str) -> bool {
        // Anything in `scheme_map` must have at least request permissions.
        self.scheme_map.contains_key(scheme)
    }

    // TODO(crbug.com/522872468): Call from the top-level `can_commit_url` and
    // `ProcessState::can_request_url` once ported to Rust.
    #[allow(dead_code)]
    fn can_commit_url(&self, url: &ffi::GURL) -> bool {
        debug_assert!(
            !url.scheme_is_blob() && !url.scheme_is_filesystem(),
            "inner_url extraction should be done already."
        );

        // Having permission to a scheme implies permission to all of its URLs.
        if self.can_commit_scheme(url.scheme()) {
            return true;
        }

        #[cfg(IS_CHROMEOS)]
        {
            // TODO(crbug.com/40226863): Use Crubit to access
            // `kExternalFileScheme` instead of a string literal (as CXX does
            // not support C++ constants).
            if url.scheme_is("externalfile") {
                return self
                    .commit_externalfile_set
                    .contains(&ffi::normalize_external_file_url(url));
            }
        }

        // Check for permission for specific origin.
        if self.can_commit_origin(&url_rust::origin::ffi::create_origin_from_gurl(url)) {
            return true;
        }

        // Unmentioned schemes are disallowed.
        false
    }

    fn has_web_ui_bindings(&self) -> bool {
        self.has_web_ui_bindings
    }

    fn has_origin_check_exemption_for_webview(&self, origin: &UniquePtr<ffi::Origin>) -> bool {
        assert!(is_valid_webview_exemption_origin(origin));
        self.webview_origin_exemption_set.contains(origin)
    }

    fn can_send_midi_message(&self) -> bool {
        self.midi_permission == MidiPermission::CanSendMidi
            || self.midi_permission == MidiPermission::CanSendMidiSysEx
    }

    fn can_send_midi_sysex_message(&self) -> bool {
        self.midi_permission == MidiPermission::CanSendMidiSysEx
    }
}

/// Zero-cost type wrapper for ProcessState that allows mutations, which only
/// exists while the ProcessState is in ProcessStateMaps' `live_process_states`.
/// The internal type is not public and is owned by the wrapper, so other
/// callers cannot create a wrapper while the state is owned by a map.
///
/// Mutable references to self.0 should not be exposed.
pub(crate) struct MutableProcessState(ProcessState);

impl MutableProcessState {
    /// Exposes the immutable APIs of ProcessState for use in various query
    /// functions, which call ProcessStateMaps.get_for_query(). Private to the
    /// module so that query functions can't call
    /// ProcessStateMaps.get_mut().as_immutable_state() and forget to look for
    /// the state in `pending_remove_states` as well.
    fn as_immutable_state(&self) -> &ProcessState {
        &self.0
    }

    /// Consumes this MutableProcessState wrapper when transferring the
    /// ProcessState to `pending_remove_states`, ensuring it remains immutable
    /// after the transfer.
    fn into_immutable(self) -> ProcessState {
        self.0
    }

    // TODO(crbug.com/568880876): Mark the process as used when it is locked to
    // a site, so that a `Locked` lock is never unused. This must match the C++
    // `ProcessState::SetProcessLock()`.
    fn set_process_lock(&mut self, lock_to_set: AssignableProcessLock) {
        let old_lock = std::mem::take(&mut self.0.process_lock);
        self.0.process_lock = old_lock.transition_to_state(lock_to_set).into();
    }

    fn set_process_is_used(&mut self) {
        let old_lock = std::mem::take(&mut self.0.process_lock);
        self.0.process_lock = old_lock.set_is_used();
    }

    fn grant_commit_origin(&mut self, origin: UniquePtr<ffi::Origin>) {
        if origin.opaque() {
            return;
        }
        self.0.origin_map.insert(origin, CommitRequestPolicy::CommitAndRequest);
    }

    fn grant_request_origin(&mut self, origin: UniquePtr<ffi::Origin>) {
        if origin.opaque() {
            return;
        }
        // Anything already in `origin_map` must have at least request
        // permission. Use `or_insert()` to avoid downgrading an existing
        // `CommitAndRequest` permission.
        self.0.origin_map.entry(origin).or_insert(CommitRequestPolicy::RequestOnly);
    }

    fn grant_commit_scheme(&mut self, scheme: &str) {
        self.0.scheme_map.insert(scheme.to_string(), CommitRequestPolicy::CommitAndRequest);
    }

    fn grant_request_scheme(&mut self, scheme: &str) {
        // Anything already in `scheme_map` must have at least request
        // permission. Use `or_insert()` to avoid downgrading an existing
        // `CommitAndRequest` permission.
        self.0.scheme_map.entry(scheme.to_string()).or_insert(CommitRequestPolicy::RequestOnly);
    }

    #[cfg(IS_CHROMEOS)]
    fn grant_request_of_external_file_url(&mut self, url: &ffi::GURL) {
        // TODO(crbug.com/40226863): Use Crubit to access `kExternalFileScheme`
        // instead of a string literal (as CXX does not support C++ constants).
        assert!(url.scheme_is("externalfile"));
        self.0.request_externalfile_set.insert(ffi::normalize_external_file_url(url));
    }

    #[cfg(IS_CHROMEOS)]
    fn grant_commit_of_external_file_url(&mut self, url: &ffi::GURL) {
        // TODO(crbug.com/40226863): Use Crubit to access `kExternalFileScheme`
        // instead of a string literal (as CXX does not support C++ constants).
        assert!(url.scheme_is("externalfile"));
        self.0.commit_externalfile_set.insert(ffi::normalize_external_file_url(url));

        // Commit access automatically implies request access.
        self.0.request_externalfile_set.insert(ffi::normalize_external_file_url(url));
    }

    fn grant_web_ui_bindings(&mut self) {
        self.0.has_web_ui_bindings = true;
    }

    fn grant_origin_check_exemption_for_webview(&mut self, origin: UniquePtr<ffi::Origin>) {
        assert!(is_valid_webview_exemption_origin(&origin));
        self.0.webview_origin_exemption_set.insert(origin);
    }

    fn grant_send_midi_message(&mut self) {
        // MidiPermission::CanSendMidiSysEx is a superset of
        // MidiPermission::CanSendMidi, so no need to update the permission for
        // that case.
        if self.0.midi_permission == MidiPermission::CannotSendMidi {
            self.0.midi_permission = MidiPermission::CanSendMidi;
        }
    }

    fn grant_send_midi_sysex_message(&mut self) {
        self.0.midi_permission = MidiPermission::CanSendMidiSysEx;
    }
}

/// Tracks whether a child process has been granted permission to only request
/// or both commit and request a given URL scheme or origin.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum CommitRequestPolicy {
    RequestOnly,
    CommitAndRequest,
}

/// Determines if a child process can send MIDI messages.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum MidiPermission {
    /// Does not permit a child process to send messages to MIDI devices.
    CannotSendMidi,
    /// Permits a child process to send messages to any MIDI device.
    CanSendMidi,
    /// Permits a child process to send system exclusive (SysEx) messages to any
    /// MIDI device. Granting this also grants `MidiPermission::CanSendMidi`.
    CanSendMidiSysEx,
}

/// Returns whether `origin` is allowed to be granted a WebView origin check
/// exemption.
fn is_valid_webview_exemption_origin(origin: &ffi::Origin) -> bool {
    // WebView origin check exemptions are only allowed for opaque origins with
    // LoadDataWithBaseURL and file origins with
    // allow_universal_access_from_file_urls.
    //
    // TODO(crbug.com/40226863): Use Crubit to access `url::kFileScheme` instead
    // of a string literal (as CXX does not support C++ constants).
    origin.opaque() || origin.scheme() == "file"
}
