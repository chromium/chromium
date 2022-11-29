// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::bindings::encoding::{
    Bits, Context, DataHeader, DataHeaderValue, MojomNumeric, DATA_HEADER_SIZE,
};
use crate::bindings::mojom::{MojomEncodable, MOJOM_NULL_POINTER, UNION_SIZE};
use crate::bindings::util;

use std::mem;
use std::ptr;
use std::vec::Vec;

use crate::system;
use crate::system::{CastHandle, Handle, UntypedHandle};

#[derive(Debug, Eq, PartialEq)]
pub enum ValidationError {
    DifferentSizedArraysInMap,
    IllegalHandle,
    IllegalMemoryRange,
    IllegalPointer,
    MessageHeaderInvalidFlags,
    MessageHeaderMissingRequestId,
    MessageHeaderUnknownMethod,
    MisalignedObject,
    UnexpectedArrayHeader,
    UnexpectedInvalidHandle,
    UnexpectedNullPointer,
    UnexpectedNullUnion,
    UnexpectedStructHeader,
}

impl ValidationError {
    pub fn as_str(self) -> &'static str {
        match self {
            ValidationError::DifferentSizedArraysInMap => {
                "VALIDATION_ERROR_DIFFERENT_SIZED_ARRAYS_IN_MAP"
            }
            ValidationError::IllegalHandle => "VALIDATION_ERROR_ILLEGAL_HANDLE",
            ValidationError::IllegalMemoryRange => "VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE",
            ValidationError::IllegalPointer => "VALIDATION_ERROR_ILLEGAL_POINTER",
            ValidationError::MessageHeaderInvalidFlags => {
                "VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS"
            }
            ValidationError::MessageHeaderMissingRequestId => {
                "VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID"
            }
            ValidationError::MessageHeaderUnknownMethod => {
                "VALIDATION_ERROR_MESSAGE_HEADER_UNKNOWN_METHOD"
            }
            ValidationError::MisalignedObject => "VALIDATION_ERROR_MISALIGNED_OBJECT",
            ValidationError::UnexpectedArrayHeader => "VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER",
            ValidationError::UnexpectedInvalidHandle => {
                "VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE"
            }
            ValidationError::UnexpectedNullPointer => "VALIDATION_ERROR_UNEXPECTED_NULL_POINTER",
            ValidationError::UnexpectedNullUnion => "VALIDATION_ERROR_UNEXPECTED_NULL_UNION",
            ValidationError::UnexpectedStructHeader => "VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER",
        }
    }
}

/// An decoding state represents the decoding logic for a single
/// Mojom object that is NOT inlined, such as a struct or an array.
pub struct DecodingState<'slice> {
    /// The buffer the state may write to.
    data: &'slice [u8],

    /// The offset of this serialized object into the overall buffer.
    global_offset: usize,

    /// The current offset within 'data'.
    offset: usize,

    /// The current bit offset within 'data'.
    bit_offset: Bits,
}

impl<'slice> DecodingState<'slice> {
    /// Create a new decoding state.
    pub fn new(buffer: &'slice [u8], offset: usize) -> DecodingState<'slice> {
        DecodingState { data: buffer, global_offset: offset, offset: 0, bit_offset: Bits(0) }
    }

    /// Align the decoding state to the next byte.
    pub fn align_to_byte(&mut self) {
        if self.bit_offset > Bits(0) {
            self.offset += 1;
            self.bit_offset = Bits(0);
        }
    }

    /// Align the decoding state to the next 'bytes' boundary.
    pub fn align_to_bytes(&mut self, bytes: usize) {
        if self.offset != 0 {
            self.offset = util::align_bytes(self.offset, bytes);
        }
    }

    /// Read a primitive from the buffer without incrementing the offset.
    fn read_in_place<T: MojomNumeric>(&mut self) -> T {
        let mut value: T = Default::default();
        debug_assert!(mem::size_of::<T>() + self.offset <= self.data.len());
        let ptr = (&self.data[self.offset..]).as_ptr();
        unsafe {
            ptr::copy_nonoverlapping(
                mem::transmute::<*const u8, *const T>(ptr),
                &mut value as *mut T,
                1,
            );
        }
        value
    }

    /// Read a primitive from the buffer and increment the offset.
    fn read<T: MojomNumeric>(&mut self) -> T {
        let value = self.read_in_place::<T>();
        self.bit_offset = Bits(0);
        self.offset += mem::size_of::<T>();
        value
    }

    /// Decode a primitive from the buffer, naturally aligning before we read.
    pub fn decode<T: MojomNumeric>(&mut self) -> T {
        self.align_to_byte();
        self.align_to_bytes(mem::size_of::<T>());
        self.read::<T>()
    }

    /// Decode a boolean value from the buffer as one bit.
    pub fn decode_bool(&mut self) -> bool {
        let offset = self.offset;
        // Check the bit by getting the set bit and checking if its non-zero
        let value = (self.data[offset] & self.bit_offset.as_set_bit()) > 0;
        self.bit_offset += Bits(1);
        let (bits, bytes) = self.bit_offset.as_bits_and_bytes();
        self.offset += bytes;
        self.bit_offset = bits;
        value
    }

    /// If we encounter a null pointer, increment past it.
    ///
    /// Returns if we skipped or not.
    pub fn skip_if_null_pointer(&mut self) -> bool {
        self.align_to_byte();
        self.align_to_bytes(8);
        let ptr = self.read_in_place::<u64>();
        if ptr == MOJOM_NULL_POINTER {
            self.offset += 8;
        }
        ptr == MOJOM_NULL_POINTER
    }

    /// If we encounter a null union, increment past it.
    ///
    /// Returns if we skipped or not.
    pub fn skip_if_null_union(&mut self) -> bool {
        self.align_to_byte();
        self.align_to_bytes(8);
        let size = self.read_in_place::<u32>();
        if size == 0 {
            self.offset += UNION_SIZE;
        }
        size == 0
    }

    /// If we encounter a null handle, increment past it.
    ///
    /// Returns if we skipped or not.
    pub fn skip_if_null_handle(&mut self) -> bool {
        self.align_to_byte();
        self.align_to_bytes(4);
        let index = self.read_in_place::<i32>();
        if index < 0 {
            self.offset += 4;
        }
        index < 0
    }

    /// If we encounter a null interface, increment past it.
    ///
    /// Returns if we skipped or not.
    pub fn skip_if_null_interface(&mut self) -> bool {
        self.align_to_byte();
        self.align_to_bytes(4);
        let index = self.read_in_place::<i32>();
        if index < 0 {
            self.offset += 8;
        }
        index < 0
    }

    /// Decode a pointer from the buffer as a global offset into the buffer.
    ///
    /// The pointer in the buffer is an offset relative to the pointer to
    /// another location in the buffer. We convert that to an absolute
    /// offset with respect to the buffer before returning. This is our
    /// defintion of a pointer.
    pub fn decode_pointer(&mut self) -> Option<u64> {
        self.align_to_byte();
        self.align_to_bytes(8);
        let current_location = (self.global_offset + self.offset) as u64;
        let offset = self.read::<u64>();
        if offset == MOJOM_NULL_POINTER {
            Some(MOJOM_NULL_POINTER)
        } else {
            offset.checked_add(current_location)
        }
    }

    /// A routine for decoding an array header.
    ///
    /// Must be called with offset zero (that is, it must be the first thing
    /// decoded). Performs numerous validation checks.
    pub fn decode_array_header<T>(&mut self) -> Result<DataHeader, ValidationError>
    where
        T: MojomEncodable,
    {
        debug_assert_eq!(self.offset, 0);
        // Make sure we can read the size first...
        if self.data.len() < mem::size_of::<u32>() {
            return Err(ValidationError::UnexpectedArrayHeader);
        }
        let bytes = self.decode::<u32>();
        if (bytes as usize) < DATA_HEADER_SIZE {
            return Err(ValidationError::UnexpectedArrayHeader);
        }
        let elems = self.decode::<u32>();
        match T::embed_size(&Default::default()).checked_mul(elems as usize) {
            Some(value) => {
                if (bytes as usize) < value.as_bytes() + DATA_HEADER_SIZE {
                    return Err(ValidationError::UnexpectedArrayHeader);
                }
            }
            None => return Err(ValidationError::UnexpectedArrayHeader),
        }
        Ok(DataHeader::new(bytes as usize, DataHeaderValue::Elements(elems)))
    }

    /// A routine for decoding an struct header.
    ///
    /// Must be called with offset zero (that is, it must be the first thing
    /// decoded). Performs numerous validation checks.
    pub fn decode_struct_header(
        &mut self,
        versions: &[(u32, u32)],
    ) -> Result<DataHeader, ValidationError> {
        debug_assert_eq!(self.offset, 0);
        // Make sure we can read the size first...
        if self.data.len() < mem::size_of::<u32>() {
            return Err(ValidationError::UnexpectedStructHeader);
        }
        let bytes = self.decode::<u32>();
        if (bytes as usize) < DATA_HEADER_SIZE {
            return Err(ValidationError::UnexpectedStructHeader);
        }
        let version = self.decode::<u32>();
        // Versioning validation: versions are generated as a sorted array of tuples, so
        // to find the version we are given by the header we use a binary search.
        match versions.binary_search_by(|val| val.0.cmp(&version)) {
            Ok(idx) => {
                let (_, size) = versions[idx];
                if bytes != size {
                    return Err(ValidationError::UnexpectedStructHeader);
                }
            }
            Err(idx) => {
                if idx == 0 {
                    panic!(
                        "Should be earliest version? \
                            Versions: {:?}, \
                            Version: {}, \
                            Size: {}",
                        versions, version, bytes
                    );
                }
                let len = versions.len();
                let (latest_version, _) = versions[len - 1];
                let (_, size) = versions[idx - 1];
                // If this is higher than any version we know, its okay for the size to be
                // bigger, but if its a version we know about, it must match the
                // size.
                if (version > latest_version && bytes < size)
                    || (version <= latest_version && bytes != size)
                {
                    return Err(ValidationError::UnexpectedStructHeader);
                }
            }
        }
        Ok(DataHeader::new(bytes as usize, DataHeaderValue::Version(version)))
    }
}

/// A struct that will encode a given Mojom object and convert it into
/// bytes and a vector of handles.
pub struct Decoder<'slice> {
    bytes: usize,
    buffer: Option<&'slice [u8]>,
    states: Vec<DecodingState<'slice>>,
    handles: Vec<UntypedHandle>,
    handles_claimed: usize, // A length that claims all handles were claimed up to this index
    max_offset: usize,      // Represents the maximum value an offset may have
}

impl<'slice> Decoder<'slice> {
    /// Create a new Decoder.
    pub fn new(buffer: &'slice [u8], handles: Vec<UntypedHandle>) -> Decoder<'slice> {
        let max_offset = buffer.len();
        Decoder {
            bytes: 0,
            buffer: Some(buffer),
            states: Vec::new(),
            handles: handles,
            handles_claimed: 0,
            max_offset: max_offset,
        }
    }

    /// Claim space in the buffer to start decoding some object.
    ///
    /// Creates a new decoding state for the object and returns a context.
    pub fn claim(&mut self, offset: usize) -> Result<Context, ValidationError> {
        // Check if the layout order is sane
        if offset < self.bytes {
            return Err(ValidationError::IllegalMemoryRange);
        }
        // Check for 8-byte alignment
        if offset & 7 != 0 {
            return Err(ValidationError::MisalignedObject);
        }
        // Bounds check on offset
        if offset > self.max_offset {
            return Err(ValidationError::IllegalPointer);
        }
        let mut buffer = self.buffer.take().expect("No buffer?");
        let space = offset - self.bytes;
        buffer = &buffer[space..];
        // Make sure we can even read the bytes in the header
        if buffer.len() < mem::size_of::<u32>() {
            return Err(ValidationError::IllegalMemoryRange);
        }
        // Read the number of bytes in the memory region according to the data header
        let mut read_size: u32 = 0;
        unsafe {
            ptr::copy_nonoverlapping(
                mem::transmute::<*const u8, *const u32>(buffer.as_ptr()),
                &mut read_size as *mut u32,
                mem::size_of::<u32>(),
            );
        }
        let size = u32::from_le(read_size) as usize;
        // Make sure the size we read is sane...
        if size > buffer.len() {
            return Err(ValidationError::IllegalMemoryRange);
        }
        // TODO(mknyszek): Check size for validation
        let (claimed, unclaimed) = buffer.split_at(size);
        self.states.push(DecodingState::new(claimed, offset));
        self.buffer = Some(unclaimed);
        self.bytes += space + size;
        Ok(Context::new(self.states.len() - 1))
    }

    /// Claims a handle at some particular index in the given handles array.
    ///
    /// Returns the handle with all type information in-tact.
    pub fn claim_handle<T: Handle + CastHandle>(
        &mut self,
        index: i32,
    ) -> Result<T, ValidationError> {
        let real_index = if index >= 0 {
            index as usize
        } else {
            return Err(ValidationError::UnexpectedInvalidHandle);
        };
        // If the index exceeds our number of handles or if we have already claimed that
        // handle
        if real_index >= self.handles.len() || real_index < self.handles_claimed {
            return Err(ValidationError::IllegalHandle);
        }
        self.handles_claimed = real_index + 1;
        let raw_handle = self.handles[real_index].get_native_handle();
        unsafe {
            self.handles[real_index].invalidate();
            Ok(T::from_untyped(system::acquire(raw_handle)))
        }
    }

    /// Immutably borrow a decoding state via Context.
    pub fn get(&self, context: &Context) -> &DecodingState<'slice> {
        &self.states[context.id()]
    }

    /// Mutably borrow a decoding state via Context.
    pub fn get_mut(&mut self, context: &Context) -> &mut DecodingState<'slice> {
        &mut self.states[context.id()]
    }
}
