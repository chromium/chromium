// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:feature";
}

use std::collections::hash_map::Entry;

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
        // Per-child security state methods
        fn add_process(child_id: ChildProcessId);
        fn remove_process(child_id: ChildProcessId);

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

fn add_process(child_id: ChildProcessId) {
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    match cpsp.process_states.entry(child_id) {
        Entry::Occupied(_) => {
            panic!("Child process {:?} has already been registered.", child_id);
        }
        Entry::Vacant(entry) => {
            entry.insert(ProcessState::new());
        }
    }
}

fn remove_process(child_id: ChildProcessId) {
    // TODO(crbug.com/482216433): Rust currently does not have a concept of a
    // "pending removal state", which would allow this to be queried but not
    // modified. This will need to be added, as right now this can be modified
    // in the pending removal state.
    let mut cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    match cpsp.process_states.entry(child_id) {
        Entry::Occupied(entry) => {
            entry.remove();
        }
        Entry::Vacant(_) => {
            panic!("Removing a process {:?} that was not previously registered.", child_id);
        }
    }
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
    cpsp.process_states.get(&child_id).is_some_and(|state| state.can_send_midi_message())
}

fn can_send_midi_sysex_message(child_id: ChildProcessId) -> bool {
    // Note: The C++ version asserts that a process cannot have SysEx permission
    // without also having normal MIDI permission. In Rust, this invariant is
    // guaranteed by construction through the `MidiPermission` tri-state enum on
    // `ProcessState`, making a runtime check unnecessary here.
    let cpsp = ChildProcessSecurityPolicyImpl::get_locked_instance();
    cpsp.process_states.get(&child_id).is_some_and(|state| state.can_send_midi_sysex_message())
}

/// Holds security state specific to each child process.
#[derive(Debug)]
pub(crate) struct ProcessState {
    /// Determines if a child process can send MIDI messages.
    midi_permission: MidiPermission,
}

impl ProcessState {
    pub(crate) fn new() -> Self {
        ProcessState { midi_permission: MidiPermission::CannotSendMidi }
    }

    fn grant_send_midi_message(&mut self) {
        // MidiPermission::CanSendMidiSysEx is a superset of
        // MidiPermission::CanSendMidi, so no need to update the permission for
        // that case.
        if self.midi_permission == MidiPermission::CannotSendMidi {
            self.midi_permission = MidiPermission::CanSendMidi;
        }
    }

    fn grant_send_midi_sysex_message(&mut self) {
        self.midi_permission = MidiPermission::CanSendMidiSysEx;
    }

    fn can_send_midi_message(&self) -> bool {
        self.midi_permission == MidiPermission::CanSendMidi
            || self.midi_permission == MidiPermission::CanSendMidiSysEx
    }

    fn can_send_midi_sysex_message(&self) -> bool {
        self.midi_permission == MidiPermission::CanSendMidiSysEx
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
