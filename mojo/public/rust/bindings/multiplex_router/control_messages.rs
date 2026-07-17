// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the control messages sent by the `MultiplexRouter`.
//! Currently, the only supported message is a disconnect notification, which
//! informs one end of an associated pair that the other end has been dropped.
//!
//! Control messages are literal Mojom messages; this file corresponds to
//! mojo/public/interfaces/bindings/pipe_control_messages.mojom.
//! We cannot use the normal generated mojom code, because that code depends
//! on the bindings crate, which would cause a circular dependency. Instead,
//! we manually define the important parts that would otherwise have been
//! generated.

chromium::import! {
  "//mojo/public/rust/system";
  "//mojo/public/rust/mojom_value_parser";
}

use mojom_value_parser::MojomParse;

use crate::message::MojomMessage;
use crate::message_header::{MessageHeader, MessageHeaderFlags};

use super::endpoint_registry::{InterfaceId, CONTROL_INTERFACE_ID};

pub const RUN_OR_CLOSE_PIPE_MESSAGE_ID: u32 = 0xFFFFFFFE;

/***************************************************************
 * These types correspond exactly to the types in
 * mojo/public/interfaces/bindings/pipe_control_messages.mojom
 * *********************************************************** */

#[derive(Debug, PartialEq, Clone, MojomParse)]
#[mojom(in_bindings_crate)] // Lets us derive `MojomParse` from the bindings crate
pub struct DisconnectReason {
    pub custom_reason: u32,
    pub description: String,
}

#[derive(Debug, PartialEq, Clone, MojomParse)]
#[mojom(in_bindings_crate)]
pub struct PeerAssociatedEndpointClosedEvent {
    pub id: u32,
    pub disconnect_reason: Option<DisconnectReason>,
}

// NOTE: PauseUntilFlushCompletes is not currently supported in the Rust
// implementation, but is defined here for compatibility with the Mojom schema.
#[derive(Debug, PartialEq, MojomParse)]
#[mojom(in_bindings_crate)]
pub struct PauseUntilFlushCompletes {
    pub flush_pipe: system::mojo_types::UntypedHandle,
}

// NOTE: FlushAsync is not currently supported in the Rust implementation,
// but is defined here for compatibility with the Mojom schema.
#[derive(Debug, PartialEq, MojomParse)]
#[mojom(in_bindings_crate)]
pub struct FlushAsync {
    pub flusher_pipe: system::mojo_types::UntypedHandle,
}

#[derive(Debug, PartialEq, MojomParse)]
#[mojom(in_bindings_crate)]
pub enum RunOrClosePipeInput {
    PeerAssociatedEndpointClosedEvent(PeerAssociatedEndpointClosedEvent),
    PauseUntilFlushCompletes(PauseUntilFlushCompletes),
    FlushAsync(FlushAsync),
}

#[derive(Debug, PartialEq, MojomParse)]
#[mojom(in_bindings_crate)]
pub struct RunOrClosePipeMessageParams {
    pub input: RunOrClosePipeInput,
}

/// Construct a `MojomMessage` containing a `PeerAssociatedEndpointClosedEvent`
/// control message.
pub fn construct_peer_endpoint_closed_message(id: InterfaceId) -> system::message::SendableMessage {
    let event = PeerAssociatedEndpointClosedEvent { id, disconnect_reason: None };
    let input = RunOrClosePipeInput::PeerAssociatedEndpointClosedEvent(event);
    let params = RunOrClosePipeMessageParams { input };

    let (payload, handles, interface_ids_offset) = mojom_value_parser::serialize(params, &());

    let header = MessageHeader::new(
        CONTROL_INTERFACE_ID,
        RUN_OR_CLOSE_PIPE_MESSAGE_ID,
        MessageHeaderFlags::default(),
        0, // request_id
        interface_ids_offset,
    );

    MojomMessage { header, payload, handles, raw_message_handle: None }.into()
}

/// Parse a control message from its wire representation.
///
/// If this function returns `None`, it means the message was invalid. In that
/// case, this function reports the bad message before returning.
pub fn parse_incoming_control_message(mut message: MojomMessage) -> Option<RunOrClosePipeInput> {
    if message.header.name != RUN_OR_CLOSE_PIPE_MESSAGE_ID {
        let _ = message.report_bad_message("Control message has incorrect message ID");
        return None;
    }

    let mut opt_handles: Vec<Option<system::mojo_types::UntypedHandle>> =
        message.handles.into_iter().map(Some).collect();

    let params: RunOrClosePipeMessageParams = match mojom_value_parser::deserialize_exact(
        &message.payload,
        &mut opt_handles,
        message.header.interface_ids_offset(),
        &(),
    ) {
        Ok(p) => p,
        Err(_) => {
            let _ = message
                .raw_message_handle
                .unwrap()
                .report_bad_message("Control message failed deserialization");
            return None;
        }
    };

    Some(params.input)
}
