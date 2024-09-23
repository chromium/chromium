// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Mojo wire format bindings and helpers
//!
//! Contains types that are layout-equivalent to their serialized
//! representation. They are the building blocks for encoding and decoding mojo
//! messages.
//!
//! Some are helpers used in generated code.

use bytemuck::{Pod, Zeroable};

pub trait DataType {}

/// The mojo message header. Messages contain this before their payload.
///
/// This matches the representation of a version 2 header. Note that, depending
/// on the message header's specified version, the serialized representation may
/// be smaller than this Rust type.
///
/// See //mojo/public/cpp/bindings/lib/message_internal.h for the corresponding
/// C++ definitions.
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct MessageHeader {
    pub header: StructHeader,
    pub interface_id: u32,
    pub name: u32,
    pub flags: MessageHeaderFlags,
    pub trace_nonce: u32,
    // Min version 1:
    pub request_id: u64,
    // Min version 2:
    pub payload: u64,
    pub payload_interface_ids: Pointer<Array<u32>>,
}

pub const MESSAGE_HEADER_V0_SIZE: usize = 24;
pub const MESSAGE_HEADER_V1_SIZE: usize = 32;
pub const MESSAGE_HEADER_V2_SIZE: usize = 48;

static_assertions::assert_eq_size!([u8; MESSAGE_HEADER_V2_SIZE], MessageHeader);

bitflags::bitflags! {
    /// See flags in //mojo/public/cpp/bindings/message.h.
    #[repr(transparent)]
    #[derive(Clone, Copy, Debug)]
    pub struct MessageHeaderFlags: u32 {
        const EXPECTS_RESPONSE = 0b001;
        const IS_RESPONSE = 0b010;
        const IS_SYNC = 0b100;
    }
}

// SAFETY: MessageHeaderFlags is repr(transparent) with `u32` which is POD, and
// has no invariants which would make any bit pattern invalid.
//
// Bitflags has a feature to allow bytemuck derives but we don't enable it.
unsafe impl Zeroable for MessageHeaderFlags {}
unsafe impl Pod for MessageHeaderFlags {}

/// A relative pointer in a mojom serialized message.
///
/// `offset` is a byte offset relative to the location of `self` in the buffer.
/// If it's 0, it is a null pointer.
///
/// Note that while this `impl Copy` for storage in a `union`, care should be
/// taken not to use it in a different context. It is only useful relative to
/// its location in a mojom struct or union.
#[derive(Debug)]
#[repr(transparent)]
pub struct Pointer<T: ?Sized> {
    pub offset: u64,
    pub _phantom: std::marker::PhantomData<*mut T>,
}

// SAFETY: `Pointer<_>` is repr(transparent) with `u64`, which is itself `Pod`.
//
// Impl'ed manually because the derive macro doesn't handle generic args.
unsafe impl<T: ?Sized + 'static> Pod for Pointer<T> {}
unsafe impl<T: ?Sized + 'static> Zeroable for Pointer<T> {}

impl<T: ?Sized> Copy for Pointer<T> {}

impl<T: ?Sized> Clone for Pointer<T> {
    fn clone(&self) -> Self {
        *self
    }
}

/// A reference to a handle transmitted in a message.
///
/// `index` is an index (*not* a byte offset) into the message's handle vector.
/// If it's `u32::MAX`, the handle is null.
///
/// Note that while this `impl Copy` for storage in a `union`, care should be
/// taken not to use it in a different context. It represents an owned handle.
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(transparent)]
pub struct HandleRef {
    pub index: u32,
}

/// A serialized reference to a mojo interface.
///
/// Note that while this `impl Copy` for storage in a `union`, care should be
/// taken not to use it in a different context. It represents an owned handle.
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct InterfaceData {
    pub handle: HandleRef,
    pub version: u32,
}

#[derive(Debug)]
#[repr(C)]
pub struct Array<T> {
    pub header: ArrayHeader,
    pub elems: [T],
}

#[derive(Copy, Clone, Debug)]
#[repr(C)]
pub struct Map<K, V> {
    pub header: StructHeader,
    pub keys: Pointer<Array<K>>,
    pub vals: Pointer<Array<V>>,
}

// SAFETY: `Map<_, _>` is repr(C) and consists of three fields which are `Pod`.
// They have size 8 and have alignment at most 8, so there are no padding bits.
// Hence all bits of `Map<_, _>` belong to one of the `Pod` fields.
//
// Impl'ed manually because the derive macro doesn't handle generic args.
unsafe impl<K: Copy + 'static, V: Copy + 'static> Pod for Map<K, V> {}
unsafe impl<K: Copy + 'static, V: Copy + 'static> Zeroable for Map<K, V> {}

#[derive(Copy, Clone, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct StructHeader {
    pub size: u32,
    pub version: u32,
}

#[derive(Copy, Clone, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct ArrayHeader {
    pub size: u32,
    pub num_elems: u32,
}

/// The required alignment of mojom objects (structs and arrays).
pub const OBJECT_ALIGNMENT: usize = 8;

pub const UNION_DATA_SIZE: usize = 16;
pub const UNION_INNER_SIZE: usize = 8;
