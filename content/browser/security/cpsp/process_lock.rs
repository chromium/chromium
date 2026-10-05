// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::process_state::ffi;
use cxx::UniquePtr;
use std::fmt::{Debug, Display};
use std::marker::PhantomData;

// ProcessLock is a core part of Site Isolation, which is used to determine
// which documents are allowed to load in a process and which site data the
// process is allowed to access, based on the SiteInfo principal.
//
// In C++, ProcessLock states are implicitly tracked via `is_invalid()`,
// `AllowsAnySite()`, and `IsLockedToSite()`, with transitions additionally
// depending on whether the process has been used to render content. In Rust,
// this lifecycle is explicitly modeled as a state machine using typestates on
// `ProcessLock<S>`:
//
// 1. `Invalid`: No `SiteInstance`s have been associated with the process yet
//    (e.g., newly created or spare processes), and access should not be granted
//    to anything.
// 2. `NotYetAssigned`: The process has been associated with a `SiteInstance`
//    that does not require a dedicated site lock (`AllowsAnySite()` is true),
//    and the process has not yet been used to render content. It can later
//    transition to `Unlocked` (once used) or be upgraded to `Locked` (if later
//    restricted to a specific site before rendering content).
// 3. `Unlocked`: A terminal state where `AllowsAnySite()` is true and the
//    process has been used to render content (e.g., default SiteInstanceGroups
//    on Android). Because it has already hosted content, it can no longer be
//    upgraded to `Locked`.
// 4. `Locked`: A terminal state where the process is strictly locked to a
//    specific site principal (`IsLockedToSite()` is true). It cannot access
//    site data from other sites, and no further transitions are allowed.
//
// These states, together with the checks in `transition_to_state()` and the
// per-state `transition()` methods, enforce the following invariants for a
// ProcessLock's life cycle:
// - A used process cannot be made more strict (locked to a site).
// - A valid process lock cannot become invalid.
// - A process cannot be locked multiple times.
//
// TODO(crbug.com/568883891): The first invariant does not yet hold for an
// `Invalid` lock that was marked as used, since `ProcessLock<Invalid>`'s
// `transition()` drops the used bit. This matches current C++ behavior.
//
// How the types in this file fit together:
// - `ProcessLock<S>`: Wraps the underlying lock with a typestate marker `S`.
//   State transitions consume `self` by value to enforce valid transitions.
// - `AssignableProcessLock`: An enum of the states a lock can be set to
//   (`NotYetAssigned`, `Locked`). Used as the input to `set_process_lock()`,
//   constructed from C++ via `AssignableProcessLock::lift()`.
// - `StatefulProcessLock`: An enum encompassing all four states (including
//   `Invalid`), stored within `ProcessState` to track a process's current lock.
//
// ProcessLock is currently defined in terms of a single SiteInfo with a process
// lock URL, but it could be possible to define it in terms of multiple
// SiteInfos that are compatible with each other.

// SAFETY: `CppProcessLock` does not utilize shared or thread-bound data, so
// *moving* it between threads is safe. Note that `CppProcessLock` is explicitly
// not `Sync` (and should not implement `Sync`) because it internally contains
// `url::Origin` (via `SiteInfo` -> `AgentClusterKey` and
// `WebExposedIsolationInfo`). Origin is not `Sync` due to lazily-initialized
// nonces in opaque origins.
#[allow(unsafe_code)]
unsafe impl Send for ffi::CppProcessLock {}

impl Display for ffi::CppProcessLock {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", ffi::to_string(self))
    }
}

// Typestate markers for CppProcessLock.

/// Represents a `ProcessLock` that has not yet been assigned a valid state.
///
/// This is the initial state for a newly created `RenderProcessHost` before
/// `SetProcessLock` is called for the first time. This state exists until the
/// partial values needed for `NotYetAssigned` (e.g., `StoragePartition`,
/// `BrowserContext`, and `WebExposedIsolationInfo`) are known. Spare processes
/// exist in this state.
///
/// It transitions to `NotYetAssigned` or `Locked` on the first call to
/// `SetProcessLock`.
pub struct Invalid;

/// Represents a `ProcessLock` where `allows_any_site()` is true, but the
/// process is still "unused". This state exists from the time we have the
/// partial state needed (e.g., StoragePartition, BrowserContext,
/// WebExposedIsolationInfo) until the process is used or locked.
///
/// This state corresponds to `RenderProcessHost::IsUnused()`. It can transition
/// to `Unlocked` (if the process is used without becoming site-restricted) or
/// `Locked` (if the process becomes restricted to a specific site).
pub struct NotYetAssigned;

/// A terminal state where `allows_any_site()` is true and the process has been
/// used.
///
/// This state is used for processes that host content but aren't restricted to
/// a specific site principal (e.g., default SiteInstanceGroups on Android). As
/// a terminal state, no further transitions are allowed.
pub struct Unlocked;

/// A terminal state where the process is strictly locked to a specific site
/// principal.
///
/// This state corresponds to `IsLockedToSite()` being true. As a terminal
/// state, no further transitions are allowed.
pub struct Locked;

/// A type-safe wrapper around a C++ ProcessLock that uses typestates to
/// represent the lock's state.
pub struct ProcessLock<S: ProcessLockState> {
    cpp_lock: UniquePtr<ffi::CppProcessLock>,
    _phantom: PhantomData<S>,
}

impl<S: ProcessLockState> ProcessLock<S> {
    fn new(cpp_lock: UniquePtr<ffi::CppProcessLock>) -> Self {
        Self { cpp_lock, _phantom: PhantomData }
    }

    fn as_ffi(&self) -> &ffi::CppProcessLock {
        &self.cpp_lock
    }

    fn mark_used(&mut self) {
        ffi::set_is_used(self.cpp_lock.pin_mut());
    }
}

impl<S: ProcessLockState> Debug for ProcessLock<S> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ProcessLock")
            .field("value", &self.to_string())
            .field("state", &std::any::type_name::<S>())
            .finish()
    }
}

impl<S: ProcessLockState> Display for ProcessLock<S> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_ffi())
    }
}

/// Each possible state of a ProcessLock, depending on whether or how it is
/// locked to particular principals.
pub trait ProcessLockState: private::Sealed {}

impl ProcessLockState for Invalid {}
impl ProcessLockState for NotYetAssigned {}
impl ProcessLockState for Unlocked {}
impl ProcessLockState for Locked {}

// This private module and the Sealed trait ensures that only this file can add
// ProcessLockStates.
mod private {
    use super::{Invalid, Locked, NotYetAssigned, Unlocked};

    pub trait Sealed {}

    impl Sealed for Invalid {}
    impl Sealed for NotYetAssigned {}
    impl Sealed for Unlocked {}
    impl Sealed for Locked {}
}

impl ProcessLock<Invalid> {
    fn new_invalid() -> Self {
        Self::new(ffi::create_invalid())
    }

    /// Consumes the invalid lock and transitions to the provided
    /// `AssignableProcessLock`. This ensures that a process lock always
    /// transitions to a valid state and that the old lock state is not left
    /// over.
    // TODO(crbug.com/568883891): This drops the used bit if the invalid lock
    // was already marked as used. Carry the used bit over (e.g., transition to
    // `Unlocked` instead of `NotYetAssigned`) so that a used process can never
    // become unused again. This must match the C++
    // `ProcessState::SetProcessLock()`.
    fn transition(self, new_state: AssignableProcessLock) -> AssignableProcessLock {
        new_state
    }
}

impl Default for ProcessLock<Invalid> {
    fn default() -> Self {
        Self::new_invalid()
    }
}

impl ProcessLock<NotYetAssigned> {
    /// Consumes the not yet assigned lock and transitions to the provided
    /// `AssignableProcessLock` if the transition is valid.
    fn transition(self, new_state: AssignableProcessLock) -> AssignableProcessLock {
        // Verify that we are not trying to update the lock with different
        // COOP/COEP information.
        assert!(
            ffi::have_same_web_exposed_isolation_info(self.as_ffi(), new_state.as_ffi()),
            "Cannot update lock with different COOP/COEP information."
        );
        match new_state {
            // TODO(crbug.com/568893320): C++ `ProcessState::SetProcessLock()`
            // still allows this for a legacy empty-URL case. Remove that
            // carveout so C++ matches Rust.
            AssignableProcessLock::NotYetAssigned(_) => {
                panic!("Cannot lock to an allows_any_site lock multiple times.");
            }
            _ => new_state,
        }
    }

    fn into_unlocked(mut self) -> ProcessLock<Unlocked> {
        self.mark_used();
        ProcessLock::new(self.cpp_lock)
    }
}

/// An enum representing a ProcessLock that can be stored in ProcessState.
#[derive(Debug)]
pub enum StatefulProcessLock {
    Invalid(ProcessLock<Invalid>),
    NotYetAssigned(ProcessLock<NotYetAssigned>),
    Unlocked(ProcessLock<Unlocked>),
    Locked(ProcessLock<Locked>),
}

impl From<AssignableProcessLock> for StatefulProcessLock {
    fn from(assignable: AssignableProcessLock) -> Self {
        match assignable {
            AssignableProcessLock::NotYetAssigned(lock) => {
                StatefulProcessLock::NotYetAssigned(lock)
            }
            AssignableProcessLock::Locked(lock) => StatefulProcessLock::Locked(lock),
        }
    }
}

impl Default for StatefulProcessLock {
    fn default() -> Self {
        StatefulProcessLock::Invalid(ProcessLock::default())
    }
}

impl StatefulProcessLock {
    pub fn as_ffi(&self) -> &ffi::CppProcessLock {
        match self {
            StatefulProcessLock::Invalid(lock) => lock.as_ffi(),
            StatefulProcessLock::NotYetAssigned(lock) => lock.as_ffi(),
            StatefulProcessLock::Unlocked(lock) => lock.as_ffi(),
            StatefulProcessLock::Locked(lock) => lock.as_ffi(),
        }
    }

    pub fn transition_to_state(self, new_state: AssignableProcessLock) -> AssignableProcessLock {
        match self {
            StatefulProcessLock::Invalid(lock) => lock.transition(new_state),
            StatefulProcessLock::NotYetAssigned(lock) => lock.transition(new_state),
            StatefulProcessLock::Unlocked(_) => {
                panic!("Cannot lock an already used process to {}", new_state.as_ffi());
            }
            StatefulProcessLock::Locked(_) => {
                panic!("Process is already locked to {}", self.as_ffi());
            }
        }
    }

    pub fn set_is_used(self) -> Self {
        match self {
            // A process can become used before it gets a lock, for example for DevTools
            // hidden targets or renderer debug URLs. This keeps the process from being
            // reused for sites that require a dedicated process.
            StatefulProcessLock::Invalid(mut lock) => {
                lock.mark_used();
                StatefulProcessLock::Invalid(lock)
            }
            StatefulProcessLock::NotYetAssigned(lock) => {
                StatefulProcessLock::Unlocked(lock.into_unlocked())
            }
            // An `Unlocked` lock is already marked as used since `into_unlocked()`
            // is the only way to get to that state.
            StatefulProcessLock::Unlocked(lock) => StatefulProcessLock::Unlocked(lock),
            StatefulProcessLock::Locked(mut lock) => {
                lock.mark_used();
                StatefulProcessLock::Locked(lock)
            }
        }
    }
}

/// An enum representing a ProcessLock that is valid as an input to
/// SetProcessLock.
///
/// `Unlocked` is not in this enum so that a process can only get to that state
/// via `StatefulProcessLock::set_is_used()`.
#[derive(Debug)]
pub enum AssignableProcessLock {
    NotYetAssigned(ProcessLock<NotYetAssigned>),
    Locked(ProcessLock<Locked>),
}

impl AssignableProcessLock {
    pub fn lift(cpp_lock: UniquePtr<ffi::CppProcessLock>) -> Self {
        assert_ne!(
            *ffi::get_process_lock_url(&cpp_lock),
            *ffi::SiteInstanceImpl::get_default_site_url(),
            "The default site URL should never be used to lock a process."
        );
        assert!(
            cpp_lock.is_unused(),
            "Cannot lock a process to an already used lock: {}",
            *cpp_lock
        );
        if cpp_lock.is_locked_to_site() {
            AssignableProcessLock::Locked(ProcessLock::new(cpp_lock))
        } else if cpp_lock.allows_any_site() {
            AssignableProcessLock::NotYetAssigned(ProcessLock::new(cpp_lock))
        } else {
            panic!("Invalid or unknown ProcessLock state for AssignableProcessLock: {}", *cpp_lock);
        }
    }

    fn as_ffi(&self) -> &ffi::CppProcessLock {
        match self {
            AssignableProcessLock::NotYetAssigned(lock) => lock.as_ffi(),
            AssignableProcessLock::Locked(lock) => lock.as_ffi(),
        }
    }
}
