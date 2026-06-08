// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines marker types used to distinguish between different
//! kinds of pending endpoints, and bound remotes/receivers.
//!
//! Pending remotes and pending receivers are identical at the type level, and
//! have almost exactly the same set of operations. The same is true for
//! Remotes and Associated Remotes, and similarly for Receivers. In order to
//! avoid having lots of duplicate code, we therefore only have one type for
//! pending endpoints that works for both remotes and receivers, and similarly
//! we have one type for remotes that works for both associated and
//! non-associated remotes.
//!
//! However, we want to distinguish between the two options at a type level, so
//! that users don't confuse the two by accident. In order to do this while
//! still having one underlying type, we make those types generic, and use a
//! generic parameter to distinguish the two. The generic parameters are ZSTs
//! which are only useful as markers.

// TODO(crbug.com/517519327): If enums are stabilized for use in generics, we
// could use that and get rid of this whole file and a lot of boilerplate.

/// Marker type used to indicate a Remote endpoint.
pub struct Remote;

/// Marker type used to indicate a Receiver endpoint.
pub struct Receiver;

/// Trait so we can distinguish between the two marker types
pub(crate) trait IsRemote {
    const IS_REMOTE: bool;
}

impl IsRemote for Remote {
    const IS_REMOTE: bool = true;
}

impl IsRemote for Receiver {
    const IS_REMOTE: bool = false;
}

/// Marker type used to indicate an Associated endpoint.
pub struct Associated;

/// Marker type used to indicate a Primary (non-associated) endpoint.
pub struct Primary;
