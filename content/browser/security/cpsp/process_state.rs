// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:feature";
}

use std::collections::hash_map::Entry;
use std::collections::HashMap;

use content_common_id_types::ChildProcessId;

use crate::ChildProcessSecurityPolicyImpl;

#[cxx::bridge(namespace = "content::rust::child_process_security_policy")]
mod ffi {
    #![allow(unsafe_code)]
    unsafe extern "C++" {
        include!("content/public/common/child_process_id.h");

        #[namespace = "content"]
        type ChildProcessId = content_common_id_types::ChildProcessId;
    }

    extern "Rust" {
        // Process state management.
        fn create_state_for_process(child_id: ChildProcessId);
        fn prepare_to_remove_state(child_id: ChildProcessId);
        fn complete_pending_state_removal(child_id: ChildProcessId);

        // Per-child process state methods.
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
    /// Determines if a child process can send MIDI messages.
    midi_permission: MidiPermission,
}

impl ProcessState {
    /// Private to the module to prevent creating new ProcessStates outside of
    /// ProcessStateMaps.
    fn new() -> Self {
        ProcessState { midi_permission: MidiPermission::CannotSendMidi }
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
