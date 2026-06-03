// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the `ArcOrWeak` type, which as the name suggests
//! contains either a strong or a weak reference to the contained value.
//!
//! This is a helper type for the greater `MultiplexRouter` type, which
//! sometimes needs to hold a weak reference to the underlying endpoint (to
//! prevent reference cycles), but sometimes needs a strong reference (to keep
//! it alive). However, the operations on the router are identical in both
//! cases, so to avoid code duplication or complicated generics we abstract
//! over the reference strength instead.
//!
//! If you find yourself wanting to use this type elsewhere, feel free to move
//! it somewhere more general (e.g. //base).

use std::sync::{Arc, Weak};

/// A type that holds either an `Arc<T>` or a `Weak<T>`
pub enum ArcOrWeak<T: ?Sized> {
    Strong(Arc<T>),
    Weak(Weak<T>),
}

impl<T: ?Sized> Clone for ArcOrWeak<T> {
    fn clone(&self) -> Self {
        match self {
            Self::Strong(arc) => Self::Strong(Arc::clone(arc)),
            Self::Weak(weak) => Self::Weak(weak.clone()),
        }
    }
}

// Not all functions are used by the router, but are included for completeness
#[allow(dead_code)]
impl<T: ?Sized> ArcOrWeak<T> {
    /// Get a strong reference to the underlying value
    pub fn to_arc(&self) -> Option<Arc<T>> {
        match self {
            Self::Strong(arc) => Some(arc.clone()),
            Self::Weak(weak) => weak.upgrade(),
        }
    }

    /// Get a weak reference to the underlying value
    #[allow(dead_code)] // Not used, but included for completenesss
    pub fn to_weak(&self) -> Weak<T> {
        match self {
            Self::Strong(arc) => Arc::downgrade(arc),
            Self::Weak(weak) => weak.clone(),
        }
    }

    /// Apply the function `f` to the contained value, if it exists, and return
    /// the result.
    pub fn with<T2>(&self, f: impl FnOnce(&T) -> T2) -> Option<T2> {
        match self {
            Self::Strong(arc) => Some(f(arc)),
            Self::Weak(weak) => weak.upgrade().map(|arc| f(&arc)),
        }
    }

    /// Create a new `ArcOrWeak` which is guaranteed to hold a strong reference
    /// to the underlying value, unless it was already dropped.
    pub fn clone_and_upgrade_if_possible(&self) -> Self {
        match self {
            Self::Strong(arc) => Self::Strong(arc.clone()),
            Self::Weak(weak) => match weak.upgrade() {
                Some(arc) => Self::Strong(arc),
                None => Self::Weak(weak.clone()),
            },
        }
    }

    /// Create a new `ArcOrWeak` which is guaranteed to hold a weak reference
    /// to the underlying value.
    pub fn clone_and_downgrade(&self) -> Self {
        match self {
            Self::Strong(arc) => Self::Weak(Arc::downgrade(arc)),
            Self::Weak(weak) => Self::Weak(weak.clone()),
        }
    }
}
