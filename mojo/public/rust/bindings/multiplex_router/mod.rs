// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the `MultiplexRouter` type, which is used as the
//! backend for sending and receiving messages by `Remote`s, `Receiver`s,
//! `AssociatedRemote`s, and `AssociatedReceiver`s.
//!
//! Each pair of `Remote` and `Receiver` corresponds to exactly one Mojo message
//! pipe. However, it is possible to attach additional
//! `AssociatedRemote`/`AssociatedReceiver` pairs to an existing pipe, rather
//! than creating a new one. In that case, messages on the underlying pipe need
//! to be tagged to indicate which pair sent it.
//!
//! The primary job of the `MultiplexRouter` is to manage those tags. When a
//! message is sent from one of the endpoints, the router attaches the tag;
//! when a message arrives, it reads the tag to determine which endpoint should
//! receive it.
//!
//! The router is also responsible for sending disconnect notifications when one
//! of an `AssociatedRemote`/`AssociatedReceiver` pair is dropped (it does not
//! need to send a notification when the original `Remote` or `Receiver` are
//! dropped, because that closes the underlying pipe).
//!
//! The `MultiplexRouter` type is not meant to be used directly, even by remotes
//! and receivers. Instead, it is accessed via a `MultiplexRouterHandle`, which
//! contains a reference to the router as well as the endpoint's interface ID.
//!
//! To provide better encapsulation, the entire `MultiplexRouter` implementation
//! is confined to this submodule, and only the handle type is exposed to the
//! rest of the crate.

mod arc_or_weak;
mod endpoint_registry;
mod handle;
mod response_sender;
mod router;

// Needed to call `MultiplexRouterHandle` methods
pub(crate) use endpoint_registry::{EndpointInfo, InterfaceId};
pub(crate) use handle::MultiplexRouterHandle;

#[doc(hidden)]
pub use response_sender::ResponseSender;
