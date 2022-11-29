// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::bindings::mojom::MOJOM_NULL_POINTER;
use crate::bindings::util;

use std::mem;
use std::ops::{Add, AddAssign, Div, Mul, Rem, Sub};
use std::ptr;
use std::vec::Vec;

use crate::system::UntypedHandle;

/// Represents some count of bits.
///
/// Used to distinguish when we have a bit and a byte
/// count. The byte count will go in a usize, while we
/// can use this structure to safely count bits without
/// running into some subtle bugs or crazy errors.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct Bits(pub usize);

impl Bits {
    /// Convert bit representation to bytes, rounding up to the nearest byte.
    pub fn as_bytes(self) -> usize {
        util::bits_to_bytes(self.0)
    }

    /// Convert to a number of bytes plus the number of bits leftover
    /// that could not fit in a full byte.
    pub fn as_bits_and_bytes(self) -> (Bits, usize) {
        (Bits(self.0 & 7), self.0 >> 3)
    }

    /// Return 1 left-shifted by the amount of bits stored here.
    ///
    /// Only guaranteed to work for up to 8 bits.
    pub fn as_set_bit(self) -> u8 {
        debug_assert!(self.0 < 8);
        1 << (self.0 & 7)
    }

    pub fn checked_mul(self, val: usize) -> Option<Bits> {
        match val.checked_mul(self.0) {
            Some(result) => Some(Bits(result)),
            None => None,
        }
    }

    /// Align the bits to some number of bytes.
    pub fn align_to_bytes(&mut self, bytes: usize) {
        self.0 = util::align_bytes(self.0, 8 * bytes);
    }
}

impl Add for Bits {
    type Output = Self;
    fn add(self, rhs: Self) -> Self {
        Bits(self.0 + rhs.0)
    }
}

impl AddAssign for Bits {
    fn add_assign(&mut self, rhs: Self) {
        self.0 += rhs.0
    }
}

impl Mul<usize> for Bits {
    type Output = Self;
    fn mul(self, rhs: usize) -> Self {
        Bits(self.0 * rhs)
    }
}

/// This trait is intended to be used by Mojom primitive values
/// in order to be identified in generic contexts.
pub trait MojomNumeric:
    Copy
    + Clone
    + Sized
    + Add<Self>
    + Sub<Self, Output = Self>
    + Mul<Self>
    + Div<Self, Output = Self>
    + Rem<Self, Output = Self>
    + PartialEq<Self>
    + Default
{
    /// Converts the primitive to a little-endian representation (the mojom
    /// endianness).
    fn to_mojom_endian(self) -> Self;
}

macro_rules! impl_mojom_numeric_for_prim {
    ($($t:ty),*) => {
        $(
        impl MojomNumeric for $t {
            fn to_mojom_endian(self) -> $t { self.to_le() }
        }
        )*
    }
}

impl_mojom_numeric_for_prim!(i8, i16, i32, i64, u8, u16, u32, u64);

impl MojomNumeric for f32 {
    fn to_mojom_endian(self) -> f32 {
        unsafe { mem::transmute::<u32, f32>(mem::transmute::<f32, u32>(self).to_le()) }
    }
}

impl MojomNumeric for f64 {
    fn to_mojom_endian(self) -> f64 {
        unsafe { mem::transmute::<u64, f64>(mem::transmute::<f64, u64>(self).to_le()) }
    }
}

/// Align to the Mojom default of 8 bytes.
pub fn align_default(bytes: usize) -> usize {
    util::align_bytes(bytes, 8)
}

/// The size in bytes of any data header.
pub const DATA_HEADER_SIZE: usize = 8;

/// A value that goes in the second u32 of a
/// a data header.
///
/// Since the data header can head many types,
/// this enum represents all the kinds of data
/// that can end up in a data header.
#[derive(Clone, Copy)]
pub enum DataHeaderValue {
    Elements(u32),
    Version(u32),
    UnionTag(u32),
}

impl DataHeaderValue {
    /// Get the raw u32 value.
    fn as_raw(self) -> u32 {
        match self {
            DataHeaderValue::Elements(v) => v,
            DataHeaderValue::Version(v) => v,
            DataHeaderValue::UnionTag(v) => v,
        }
    }
}

/// A data header is placed at the beginning of every serialized
/// Mojom object, providing its size as well as some extra meta-data.
///
/// The meta-data should always come from a DataHeaderValue.
pub struct DataHeader {
    size: u32,
    data: u32,
}

impl DataHeader {
    /// Create a new DataHeader.
    pub fn new(size: usize, data: DataHeaderValue) -> DataHeader {
        DataHeader { size: size as u32, data: data.as_raw() }
    }

    /// Getter for size.
    pub fn size(&self) -> u32 {
        self.size
    }

    /// Getter for extra meta-data.
    pub fn data(&self) -> u32 {
        self.data
    }
}

/// This context object represents an encoding/decoding context.
#[derive(Clone, Default)]
pub struct Context {
    /// An index representing an encoding state.
    id: usize,

    /// Whether or not our current context is directly inside of
    /// a union.
    is_union: bool,
}

impl Context {
    /// Create a new context with all data default.
    pub fn new(id: usize) -> Context {
        Context { id: id, is_union: false }
    }

    /// Getter for the encoding state ID.
    pub fn id(&self) -> usize {
        self.id
    }

    /// Getter for whether or not we are in a union.
    pub fn is_union(&self) -> bool {
        self.is_union
    }

    /// Change whether or not we are inside of a union and create that
    /// as a new context.
    pub fn set_is_union(&self, value: bool) -> Context {
        let mut new_context = self.clone();
        new_context.is_union = value;
        new_context
    }
}

/// An encoding state represents the encoding logic for a single
/// Mojom object that is NOT inlined, such as a struct or an array.
pub struct EncodingState<'slice> {
    /// The buffer the state may write to.
    data: &'slice mut [u8],

    /// The offset of this serialized object into the overall buffer.
    global_offset: usize,

    /// The current offset within 'data'.
    offset: usize,

    /// The current bit offset within 'data'.
    bit_offset: Bits,
}

impl<'slice> EncodingState<'slice> {
    /// Create a new encoding state.
    ///
    /// Note: the encoder will not allocate a buffer for you, rather
    /// a pre-allocated buffer must be passed in.
    pub fn new(
        buffer: &'slice mut [u8],
        header: &DataHeader,
        offset: usize,
    ) -> EncodingState<'slice> {
        let mut state =
            EncodingState { data: buffer, global_offset: offset, offset: 0, bit_offset: Bits(0) };
        state.write(header.size());
        state.write(header.data());
        state
    }

    /// Align the encoding state to the next byte.
    pub fn align_to_byte(&mut self) {
        if self.bit_offset > Bits(0) {
            self.offset += 1;
            self.bit_offset = Bits(0);
        }
    }

    /// Align the encoding state to the next 'bytes' boundary.
    pub fn align_to_bytes(&mut self, bytes: usize) {
        self.offset = util::align_bytes(self.offset, bytes);
    }

    /// Write a primitive into the buffer.
    fn write<T: MojomNumeric>(&mut self, data: T) {
        let num_bytes = mem::size_of::<T>();
        let bytes = data.to_mojom_endian();
        debug_assert!(num_bytes + self.offset <= self.data.len());
        unsafe {
            ptr::copy_nonoverlapping(
                mem::transmute::<&T, *const u8>(&bytes),
                (&mut self.data[self.offset..]).as_mut_ptr(),
                num_bytes,
            );
        }
        self.bit_offset = Bits(0);
        self.offset += num_bytes;
    }

    /// Encode a primitive into the buffer, naturally aligning it.
    pub fn encode<T: MojomNumeric>(&mut self, data: T) {
        self.align_to_byte();
        self.align_to_bytes(mem::size_of::<T>());
        self.write(data);
    }

    /// Encode a boolean value into the buffer as one bit.
    pub fn encode_bool(&mut self, data: bool) {
        let offset = self.offset;
        if data {
            self.data[offset] |= self.bit_offset.as_set_bit();
        }
        self.bit_offset += Bits(1);
        let (bits, bytes) = self.bit_offset.as_bits_and_bytes();
        self.offset += bytes;
        self.bit_offset = bits;
    }

    /// Encode a null union into the buffer.
    pub fn encode_null_union(&mut self) {
        self.align_to_byte();
        self.align_to_bytes(8);
        self.write(0 as u32); // Size
        self.write(0 as u32); // Tag
        self.write(0 as u64); // Data
    }

    /// Encode a null pointer into the buffer.
    pub fn encode_null_pointer(&mut self) {
        self.align_to_byte();
        self.align_to_bytes(8);
        self.encode(MOJOM_NULL_POINTER);
    }

    /// Encode a null handle into the buffer.
    pub fn encode_null_handle(&mut self) {
        self.align_to_byte();
        self.align_to_bytes(4);
        self.encode(-1 as i32);
    }

    /// Encode a non-null pointer into the buffer.
    ///
    /// 'location' is an absolute location in the global buffer, but
    /// Mojom pointers are offsets relative to the pointer, so we
    /// perform that conversion here before writing.
    pub fn encode_pointer(&mut self, location: u64) {
        self.align_to_byte();
        self.align_to_bytes(8);
        let current_location = (self.global_offset + self.offset) as u64;
        debug_assert!(location >= current_location);
        self.encode(location - current_location);
    }
}

/// A struct that will encode a given Mojom object and convert it into
/// bytes and a vector of handles.
pub struct Encoder<'slice> {
    bytes: usize,
    buffer: Option<&'slice mut [u8]>,
    states: Vec<EncodingState<'slice>>,
    handles: Vec<UntypedHandle>,
}

impl<'slice> Encoder<'slice> {
    /// Create a new Encoder.
    pub fn new(buffer: &'slice mut [u8]) -> Encoder<'slice> {
        Encoder { bytes: 0, buffer: Some(buffer), states: Vec::new(), handles: Vec::new() }
    }

    /// Get the current encoded size (useful for writing pointers).
    pub fn size(&self) -> usize {
        self.bytes
    }

    /// Start encoding a new object with its data header.
    ///
    /// Creates a new encoding state for the object.
    pub fn add(&mut self, header: &DataHeader) -> Option<Context> {
        let buf = self.buffer.take().unwrap();
        if buf.len() < (header.size() as usize) {
            self.buffer = Some(buf);
            return None;
        }
        let obj_bytes = header.size() as usize;
        let (claimed, rest) = buf.split_at_mut(obj_bytes);
        self.states.push(EncodingState::new(claimed, header, self.bytes));
        self.bytes += obj_bytes;
        let padding_bytes = align_default(obj_bytes) - obj_bytes;
        if padding_bytes <= rest.len() {
            let (_, new_buffer) = rest.split_at_mut(padding_bytes);
            self.bytes += padding_bytes;
            self.buffer = Some(new_buffer);
        } else {
            self.buffer = Some(rest);
        }
        Some(Context::new(self.states.len() - 1))
    }

    /// Adds a handle and returns an offset to that handle in the
    /// final handle vector.
    pub fn add_handle(&mut self, handle: UntypedHandle) -> usize {
        self.handles.push(handle);
        self.handles.len() - 1
    }

    /// Immutably borrow an encoding state via Context.
    pub fn get(&self, context: &Context) -> &EncodingState<'slice> {
        &self.states[context.id()]
    }

    /// Mutably borrow an encoding state via Context.
    pub fn get_mut(&mut self, context: &Context) -> &mut EncodingState<'slice> {
        &mut self.states[context.id()]
    }

    /// Signal to finish encoding by destroying the Encoder and returning the
    /// final handle vector.
    ///
    /// Note: No byte buffer is returned as that is pre-allocated.
    pub fn unwrap(self) -> Vec<UntypedHandle> {
        self.handles
    }
}
