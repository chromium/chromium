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

/// A reference to a handle transmitted in a message.
///
/// `index` is an index (*not* a byte offset) into the message's handle vector.
/// If it's `u32::MAX`, the handle is null.
///
/// Note that while this `impl Copy` for storage in a `union`, care should be
/// taken not to use it in a different context. It represents an owned handle.
#[derive(Clone, Copy, Debug)]
#[repr(transparent)]
pub struct HandleRef {
    pub index: u32,
}

/// A serialized reference to a mojo interface.
///
/// Note that while this `impl Copy` for storage in a `union`, care should be
/// taken not to use it in a different context. It represents an owned handle.
#[derive(Clone, Copy, Debug)]
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

#[derive(Debug)]
pub struct Map<K, V> {
    pub header: StructHeader,
    pub keys: Pointer<Array<K>>,
    pub vals: Pointer<Array<V>>,
}

#[derive(Debug)]
#[repr(C)]
pub struct StructHeader {
    pub size: u32,
    pub version: u32,
}

#[derive(Debug)]
#[repr(C)]
pub struct ArrayHeader {
    pub size: u32,
    pub num_elems: u32,
}

impl<T: ?Sized> Copy for Pointer<T> {}

impl<T: ?Sized> Clone for Pointer<T> {
    fn clone(&self) -> Self {
        *self
    }
}

pub const UNION_DATA_SIZE: usize = 16;
pub const UNION_INNER_SIZE: usize = 8;
