// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::bindings::decoding::{Decoder, ValidationError};
use crate::bindings::encoding;
use crate::bindings::encoding::{
    Bits, Context, DataHeader, DataHeaderValue, Encoder, EncodingState, DATA_HEADER_SIZE,
};
use crate::bindings::message::MessageHeader;

use std::cmp::Eq;
use std::collections::HashMap;
use std::hash::Hash;
use std::mem;
use std::panic;
use std::vec::Vec;

use crate::system::data_pipe;
use crate::system::message_pipe;
use crate::system::shared_buffer;
use crate::system::{CastHandle, Handle, MojoResult, UntypedHandle};

/// The size of a Mojom map plus header in bytes.
const MAP_SIZE: usize = 24;

/// The sorted set of versions for a map.
const MAP_VERSIONS: [(u32, u32); 1] = [(0, MAP_SIZE as u32)];

/// The size of a Mojom union in bytes (header included).
pub const UNION_SIZE: usize = 16;

/// The size of a Mojom pointer in bits.
pub const POINTER_BIT_SIZE: Bits = Bits(64);

/// The value of a Mojom null pointer.
pub const MOJOM_NULL_POINTER: u64 = 0;

/// An enumeration of all the possible low-level Mojom types.
pub enum MojomType {
    Simple,
    Pointer,
    Union,
    Handle,
    Interface,
}

/// Whatever implements this trait can be serialized in the Mojom format.
pub trait MojomEncodable: Sized {
    /// Get the Mojom type.
    fn mojom_type() -> MojomType;

    /// Get this type's Mojom alignment.
    fn mojom_alignment() -> usize;

    /// The amount of space in bits the type takes up when inlined
    /// into another type at serialization time.
    fn embed_size(context: &Context) -> Bits;

    /// Recursively computes the size of the complete Mojom archive
    /// starting from this type.
    fn compute_size(&self, context: Context) -> usize;

    /// Encodes this type into the encoder given a context.
    fn encode(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context);

    /// Using a decoder, decodes itself out of a byte buffer.
    fn decode(decoder: &mut Decoder, context: Context) -> Result<Self, ValidationError>;
}

/// Whatever implements this trait is a Mojom pointer type which means
/// that on encode, a pointer is inlined and the implementer is
/// serialized elsewhere in the output buffer.
pub trait MojomPointer: MojomEncodable {
    /// Get the DataHeader meta-data for this pointer type.
    fn header_data(&self) -> DataHeaderValue;

    /// Get the size of only this type when serialized.
    fn serialized_size(&self, context: &Context) -> usize;

    /// Encodes the actual values of the type into the encoder.
    fn encode_value(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context);

    /// Decodes the actual values of the type into the decoder.
    fn decode_value(decoder: &mut Decoder, context: Context) -> Result<Self, ValidationError>;

    /// Writes a pointer inlined into the current context before calling
    /// encode_value.
    #[must_use]
    fn encode_new(self, encoder: &mut Encoder, context: Context) -> u64 {
        let data_size = self.serialized_size(&context);
        let data_header = DataHeader::new(data_size, self.header_data());
        let (offset, mut state, new_context) = encoder.add(&data_header).unwrap();
        self.encode_value(encoder, &mut state, new_context);
        offset
    }

    /// Reads a pointer inlined into the current context before calling
    /// decode_value.
    fn decode_new(
        decoder: &mut Decoder,
        _context: Context,
        pointer: u64,
    ) -> Result<Self, ValidationError> {
        match decoder.claim(pointer as usize) {
            Ok(new_context) => Self::decode_value(decoder, new_context),
            Err(err) => Err(err),
        }
    }
}

/// Whatever implements this trait is a Mojom union type which means that
/// on encode it is inlined, but if the union is nested inside of another
/// union type, it is treated as a pointer type.
pub trait MojomUnion: MojomEncodable {
    /// Get the union's current tag.
    fn get_tag(&self) -> u32;

    /// Encode the actual value of the union.
    fn encode_value(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context);

    /// Decode the actual value of the union.
    fn decode_value(decoder: &mut Decoder, context: Context) -> Result<Self, ValidationError>;
}

pub const UNION_NESTED_EMBED_SIZE: Bits = POINTER_BIT_SIZE;
pub const UNION_INLINE_EMBED_SIZE: Bits = Bits(8 * (UNION_SIZE as usize));

/// Encode a union indirectly by pointer.
pub fn encode_union_nested<T: MojomUnion>(
    val: T,
    encoder: &mut Encoder,
    state: &mut EncodingState,
    _context: Context,
) {
    let tag = DataHeaderValue::UnionTag(val.get_tag());
    let data_header = DataHeader::new(UNION_SIZE, tag);
    let (offset, mut new_state, new_context) = encoder.add(&data_header).unwrap();
    state.encode_pointer(offset);
    val.encode_value(encoder, &mut new_state, new_context.set_is_union(true));
}

/// Encode a union directly in the current context.
pub fn encode_union_inline<T: MojomUnion>(
    val: T,
    encoder: &mut Encoder,
    state: &mut EncodingState,
    context: Context,
) {
    state.align_to_bytes(8);
    state.encode(UNION_SIZE as u32);
    state.encode(val.get_tag());
    val.encode_value(encoder, state, context.clone());
    state.align_to_bytes(8);
    state.align_to_byte();
}

/// Decode a union that is referenced by pointer.
pub fn decode_union_nested<T: MojomUnion>(
    decoder: &mut Decoder,
    context: Context,
) -> Result<T, ValidationError> {
    let global_offset = {
        let state = decoder.get_mut(&context);
        match state.decode_pointer() {
            Some(ptr) => ptr as usize,
            None => return Err(ValidationError::IllegalPointer),
        }
    };
    if global_offset == (MOJOM_NULL_POINTER as usize) {
        return Err(ValidationError::UnexpectedNullPointer);
    }
    match decoder.claim(global_offset as usize) {
        Ok(new_context) => T::decode_value(decoder, new_context),
        Err(err) => Err(err),
    }
}

/// Decode a union stored inline in the current context.
pub fn decode_union_inline<T: MojomUnion>(
    decoder: &mut Decoder,
    context: Context,
) -> Result<T, ValidationError> {
    {
        let state = decoder.get_mut(&context);
        state.align_to_byte();
        state.align_to_bytes(8);
    }
    let value = T::decode_value(decoder, context.clone());
    {
        let state = decoder.get_mut(&context);
        state.align_to_byte();
        state.align_to_bytes(8);
    }
    value
}

/// A marker trait that marks Mojo handles as encodable.
pub trait MojomHandle: CastHandle + MojomEncodable {}

/// Whatever implements this trait is considered to be a Mojom
/// interface, that is, a message pipe which conforms to some
/// messaging interface.
///
/// We force an underlying message pipe to be used via the pipe()
/// and unwrap() routines.
pub trait MojomInterface: MojomEncodable {
    /// Get the service name for this interface.
    fn service_name() -> &'static str;

    /// Get the version for this interface.
    fn version(&self) -> u32;

    /// Access the underlying message pipe for this interface.
    fn pipe(&self) -> &message_pipe::MessageEndpoint;

    /// Unwrap the interface into its underlying message pipe.
    fn unwrap(self) -> message_pipe::MessageEndpoint;
}

/// An error that may occur when sending data over a Mojom interface.
#[derive(Debug)]
pub enum MojomSendError {
    /// Failed to write to the underlying message pipe.
    FailedWrite(MojoResult),

    /// The version is too old to write the attempted message.
    OldVersion(u32, u32),
}

/// Whatever implements this trait is considered to be a Mojom
/// interface that may send messages of some generic type.
///
/// When implementing this trait, the correct way is to specify
/// a tighter trait bound than MojomMessage that limits the types
/// available for sending to those that are valid messages available
/// to the interface.
///
/// TODO(mknyszek): Add sending control messages
pub trait MojomInterfaceSend<R: MojomMessage>: MojomInterface {
    /// Creates a message.
    fn create_request(&self, req_id: u64, payload: R) -> (Vec<u8>, Vec<UntypedHandle>) {
        let mut header = R::create_header();
        header.request_id = req_id;
        let header_size = header.compute_size(Default::default());
        let size = header_size + payload.compute_size(Default::default());
        let mut buffer: Vec<u8> = Vec::with_capacity(size);
        buffer.resize(size, 0);
        let handles = {
            let (header_buf, rest_buf) = buffer.split_at_mut(header_size);
            let mut handles = header.serialize(header_buf);
            handles.extend(payload.serialize(rest_buf).into_iter());
            handles
        };
        (buffer, handles)
    }

    /// Creates and sends a message, and returns its request ID.
    fn send_request(&self, req_id: u64, payload: R) -> Result<(), MojomSendError> {
        if self.version() < R::min_version() {
            return Err(MojomSendError::OldVersion(self.version(), R::min_version()));
        }
        let (buffer, handles) = self.create_request(req_id, payload);
        match self.pipe().write(&buffer, handles) {
            MojoResult::Okay => Ok(()),
            err => Err(MojomSendError::FailedWrite(err)),
        }
    }
}

/// An error that may occur when attempting to recieve a message over a
/// Mojom interface.
#[derive(Debug)]
pub enum MojomRecvError {
    /// Failed to read from the underlying message pipe.
    FailedRead(MojoResult),

    /// Failed to validate the buffer during decode.
    FailedValidation(ValidationError),
}

/// Whatever implements this trait is considered to be a Mojom
/// interface that may recieve messages for some interface.
///
/// When implementing this trait, specify the container "union" type
/// which can contain any of the potential messages that may be recieved.
/// This way, we can return that type and let the user multiplex over
/// what message was received.
///
/// TODO(mknyszek): Add responding to control messages
pub trait MojomInterfaceRecv: MojomInterface {
    type Container: MojomMessageOption;

    /// Tries to read a message from a pipe and decodes it.
    fn recv_response(&self) -> Result<(u64, Self::Container), MojomRecvError> {
        match self.pipe().read() {
            Ok((buffer, handles)) => match Self::Container::decode_message(buffer, handles) {
                Ok((req_id, val)) => Ok((req_id, val)),
                Err(err) => Err(MojomRecvError::FailedValidation(err)),
            },
            Err(err) => Err(MojomRecvError::FailedRead(err)),
        }
    }
}

/// Whatever implements this trait is considered to be a Mojom struct.
///
/// Mojom structs are always the root of any Mojom message. Thus, we
/// provide convenience functions for serialization here.
pub trait MojomStruct: MojomPointer {
    /// Given a pre-allocated buffer, the struct serializes itself.
    fn serialize(self, buffer: &mut [u8]) -> Vec<UntypedHandle> {
        let mut encoder = Encoder::new(buffer);
        // The root object is not referred to by pointer, so we can ignore the
        // offset.
        let _ = self.encode_new(&mut encoder, Default::default());
        encoder.unwrap()
    }

    /// The struct computes its own size, allocates a buffer, and then
    /// serializes itself into that buffer.
    fn auto_serialize(self) -> (Vec<u8>, Vec<UntypedHandle>) {
        let size = self.compute_size(Default::default());
        let mut buf = Vec::with_capacity(size);
        buf.resize(size, 0);
        let handles = self.serialize(&mut buf);
        (buf, handles)
    }

    /// Decode the type from a byte array and a set of handles.
    fn deserialize(buffer: &[u8], handles: Vec<UntypedHandle>) -> Result<Self, ValidationError> {
        let mut decoder = Decoder::new(buffer, handles);
        Self::decode_new(&mut decoder, Default::default(), 0)
    }
}

/// Marks a MojomStruct as being capable of being sent across some
/// Mojom interface.
pub trait MojomMessage: MojomStruct {
    fn min_version() -> u32;
    fn create_header() -> MessageHeader;
}

/// The trait for a "container" type intended to be used in MojomInterfaceRecv.
///
/// This trait contains the decode logic which decodes based on the message
/// header and returns itself: a union type which may contain any of the
/// possible messages that may be sent across this interface.
pub trait MojomMessageOption: Sized {
    /// Decodes the actual payload of the message.
    ///
    /// Implemented by a code generator.
    fn decode_payload(
        header: MessageHeader,
        buffer: &[u8],
        handles: Vec<UntypedHandle>,
    ) -> Result<Self, ValidationError>;

    /// Decodes the message header and then the payload, returning a new
    /// copy of itself and the request ID found in the header.
    fn decode_message(
        buffer: Vec<u8>,
        handles: Vec<UntypedHandle>,
    ) -> Result<(u64, Self), ValidationError> {
        let header = MessageHeader::deserialize(&buffer[..], Vec::new())?;
        let payload_buffer = &buffer[header.serialized_size(&Default::default())..];
        let req_id = header.request_id;
        let ret = Self::decode_payload(header, payload_buffer, handles)?;
        Ok((req_id, ret))
    }
}

// ********************************************** //
// ****** IMPLEMENTATIONS FOR COMMON TYPES ****** //
// ********************************************** //

macro_rules! impl_encodable_for_prim {
    ($($prim_type:ty),*) => {
        $(
        impl MojomEncodable for $prim_type {
            fn mojom_type() -> MojomType {
                MojomType::Simple
            }
            fn mojom_alignment() -> usize {
                mem::size_of::<$prim_type>()
            }
            fn embed_size(_context: &Context) -> Bits {
                Bits(8 * mem::size_of::<$prim_type>())
            }
            fn compute_size(&self, _context: Context) -> usize {
                0 // Indicates that this type is inlined and it adds nothing external to the size
            }
            fn encode(self, _encoder: &mut Encoder, state: &mut EncodingState, _context: Context) {
                state.encode(self);
            }
            fn decode(decoder: &mut Decoder, context: Context) -> Result<Self, ValidationError> {
                let state = decoder.get_mut(&context);
                Ok(state.decode::<Self>())
            }
        }
        )*
    }
}

impl_encodable_for_prim!(i8, i16, i32, i64, u8, u16, u32, u64, f32, f64);

impl MojomEncodable for bool {
    fn mojom_alignment() -> usize {
        panic!("Should never check_decode mojom_alignment of bools (they're bit-aligned)!");
    }
    fn mojom_type() -> MojomType {
        MojomType::Simple
    }
    fn embed_size(_context: &Context) -> Bits {
        Bits(1)
    }
    fn compute_size(&self, _context: Context) -> usize {
        0 // Indicates that this type is inlined and it adds nothing external to the size
    }
    fn encode(self, _encoder: &mut Encoder, state: &mut EncodingState, _context: Context) {
        state.encode_bool(self);
    }
    fn decode(decoder: &mut Decoder, context: Context) -> Result<Self, ValidationError> {
        let state = decoder.get_mut(&context);
        Ok(state.decode_bool())
    }
}

// Options should be considered to represent nullability the Mojom IDL.
// Any type wrapped in an Option type is nullable.

impl<T: MojomEncodable> MojomEncodable for Option<T> {
    fn mojom_alignment() -> usize {
        T::mojom_alignment()
    }
    fn mojom_type() -> MojomType {
        T::mojom_type()
    }
    fn embed_size(context: &Context) -> Bits {
        T::embed_size(context)
    }
    fn compute_size(&self, context: Context) -> usize {
        match *self {
            Some(ref value) => value.compute_size(context),
            None => 0,
        }
    }
    fn encode(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context) {
        match self {
            Some(value) => value.encode(encoder, state, context),
            None => match T::mojom_type() {
                MojomType::Pointer => state.encode_null_pointer(),
                MojomType::Union => state.encode_null_union(),
                MojomType::Handle => state.encode_null_handle(),
                MojomType::Interface => {
                    state.encode_null_handle();
                    state.encode(0 as u32);
                }
                MojomType::Simple => panic!("Unexpected simple type in Option!"),
            },
        }
    }
    fn decode(decoder: &mut Decoder, context: Context) -> Result<Self, ValidationError> {
        let skipped = {
            let state = decoder.get_mut(&context);
            match T::mojom_type() {
                MojomType::Pointer => state.skip_if_null_pointer(),
                MojomType::Union => state.skip_if_null_union(),
                MojomType::Handle => state.skip_if_null_handle(),
                MojomType::Interface => state.skip_if_null_interface(),
                MojomType::Simple => panic!("Unexpected simple type in Option!"),
            }
        };
        if skipped {
            Ok(None)
        } else {
            match T::decode(decoder, context) {
                Ok(value) => Ok(Some(value)),
                Err(err) => Err(err),
            }
        }
    }
}

macro_rules! impl_pointer_for_array {
    () => {
        fn header_data(&self) -> DataHeaderValue {
            DataHeaderValue::Elements(self.len() as u32)
        }
        fn serialized_size(&self, context: &Context) -> usize {
            DATA_HEADER_SIZE
                + if self.len() > 0 { (T::embed_size(context) * self.len()).as_bytes() } else { 0 }
        }
    };
}

macro_rules! impl_encodable_for_array {
    () => {
        impl_encodable_for_pointer!();
        fn compute_size(&self, context: Context) -> usize {
            let mut size = encoding::align_default(self.serialized_size(&context));
            for elem in self.iter() {
                size += elem.compute_size(context.clone());
            }
            size
        }
    };
}

impl<T: MojomEncodable> MojomPointer for Vec<T> {
    impl_pointer_for_array!();
    fn encode_value(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context) {
        for elem in self.into_iter() {
            elem.encode(encoder, state, context.clone());
        }
    }
    fn decode_value(decoder: &mut Decoder, context: Context) -> Result<Vec<T>, ValidationError> {
        let elems = {
            let state = decoder.get_mut(&context);
            match state.decode_array_header::<T>() {
                Ok(header) => header.data(),
                Err(err) => return Err(err),
            }
        };
        let mut value = Vec::with_capacity(elems as usize);
        for _ in 0..elems {
            match T::decode(decoder, context.clone()) {
                Ok(elem) => value.push(elem),
                Err(err) => return Err(err),
            }
        }
        Ok(value)
    }
}

impl<T: MojomEncodable> MojomEncodable for Vec<T> {
    impl_encodable_for_array!();
}

impl<T: MojomEncodable, const N: usize> MojomPointer for [T; N] {
    impl_pointer_for_array!();
    fn encode_value(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context) {
        for elem in self.into_iter() {
            let next_context = context.clone();
            elem.encode(encoder, state, next_context);
        }
    }

    fn decode_value(decoder: &mut Decoder, context: Context) -> Result<[T; N], ValidationError> {
        let len = {
            let state = decoder.get_mut(&context);
            match state.decode_array_header::<T>() {
                Ok(header) => header.data(),
                Err(err) => return Err(err),
            }
        };
        if len as usize != N {
            return Err(ValidationError::UnexpectedArrayHeader);
        }

        // Mojom objects are encoded and decoded "by-value", since they can
        // transfer handles which are owned. Since there is no default value for
        // T, we must initialize the array element-by-element.
        let mut array_uninit: [mem::MaybeUninit<T>; N] = unsafe {
            // We call assume_init() for a MaybeUninit<[MaybeUnint]> which drops the outer
            // MaybeUninit, producing a "initialized" array of MaybeUnint elements. This is
            // fine since MaybeUninit does not actually store whether it's initialized and
            // our code hereafter continues to assume the elements are not initialized yet.
            // See https://doc.rust-lang.org/stable/std/mem/union.MaybeUninit.html#initializing-an-array-element-by-element
            mem::MaybeUninit::uninit().assume_init()
        };

        for elem in &mut array_uninit {
            let next_context = context.clone();

            // This is unwind-safe. If `T::decode` panics, we simply leak the
            // previously-decoded array elements. Since it's an array of
            // `MaybeUninit<T>` drop will not be called.
            //
            // Leaking values is OK since panics are intended to be
            // unrecoverable.
            elem.write(T::decode(decoder, next_context)?);
        }

        // SAFETY:
        // * Every `MaybeUninit<T>` element of `array_uninit` has been initialized,
        //   since in our loop above looped over every element and wrote a valid value.
        // * Transmute from `[MaybeUninit<T>; N]` to `[T; N]` is safe if all elements
        //   are initialized.
        //
        // Unfortunately regular transmute doesn't work: it can't handle types
        // parameterized by generic T, even though the arrays are the same size.
        // Known issue: https://github.com/rust-lang/rust/issues/47966
        let array =
            unsafe { mem::transmute_copy::<[mem::MaybeUninit<T>; N], [T; N]>(&array_uninit) };
        Ok(array)
    }
}
impl<T: MojomEncodable, const N: usize> MojomEncodable for [T; N] {
    impl_encodable_for_array!();
}

impl<T: MojomEncodable> MojomPointer for Box<[T]> {
    impl_pointer_for_array!();
    fn encode_value(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context) {
        for elem in self.into_vec().into_iter() {
            elem.encode(encoder, state, context.clone());
        }
    }
    fn decode_value(decoder: &mut Decoder, context: Context) -> Result<Box<[T]>, ValidationError> {
        match Vec::<T>::decode_value(decoder, context) {
            Ok(vec) => Ok(vec.into_boxed_slice()),
            Err(err) => Err(err),
        }
    }
}

impl<T: MojomEncodable> MojomEncodable for Box<[T]> {
    impl_encodable_for_array!();
}

// We can represent a Mojom string as just a Rust String type
// since both are UTF-8.
impl MojomPointer for String {
    fn header_data(&self) -> DataHeaderValue {
        DataHeaderValue::Elements(self.len() as u32)
    }
    fn serialized_size(&self, _context: &Context) -> usize {
        DATA_HEADER_SIZE + self.len()
    }
    fn encode_value(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context) {
        for byte in self.as_bytes() {
            byte.encode(encoder, state, context.clone());
        }
    }
    fn decode_value(decoder: &mut Decoder, context: Context) -> Result<String, ValidationError> {
        let state = decoder.get_mut(&context);
        let elems = match state.decode_array_header::<u8>() {
            Ok(header) => header.data(),
            Err(err) => return Err(err),
        };
        let mut value = Vec::with_capacity(elems as usize);
        for _ in 0..elems {
            value.push(state.decode::<u8>());
        }
        match String::from_utf8(value) {
            Ok(string) => Ok(string),
            Err(err) => panic!("Error decoding String: {}", err),
        }
    }
}

impl MojomEncodable for String {
    impl_encodable_for_pointer!();
    fn compute_size(&self, context: Context) -> usize {
        encoding::align_default(self.serialized_size(&context))
    }
}

/// Helper function to clean up duplicate code in HashMap.
fn array_claim_and_decode_header<T: MojomEncodable>(
    decoder: &mut Decoder,
    offset: usize,
) -> Result<(Context, usize), ValidationError> {
    let context = match decoder.claim(offset) {
        Ok(new_context) => new_context,
        Err(err) => return Err(err),
    };
    let elems = {
        let state = decoder.get_mut(&context);
        match state.decode_array_header::<T>() {
            Ok(header) => header.data(),
            Err(err) => return Err(err),
        }
    };
    Ok((context, elems as usize))
}

impl<K: MojomEncodable + Eq + Hash, V: MojomEncodable> MojomPointer for HashMap<K, V> {
    fn header_data(&self) -> DataHeaderValue {
        DataHeaderValue::Version(0)
    }
    fn serialized_size(&self, _context: &Context) -> usize {
        MAP_SIZE
    }
    fn encode_value(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context) {
        let elems = self.len();
        let meta_value = DataHeaderValue::Elements(elems as u32);
        // We need to move values into this vector because we can't copy the keys.
        // (Handles are not copyable so MojomEncodable cannot be copyable!)
        let mut vals_vec = Vec::with_capacity(elems);
        // Key setup
        // Create the keys data header
        let keys_bytes = DATA_HEADER_SIZE + (K::embed_size(&context) * elems).as_bytes();
        let keys_data_header = DataHeader::new(keys_bytes, meta_value);
        // Claim space for the keys array in the encoder
        let (keys_offset, mut keys_state, keys_context) = encoder.add(&keys_data_header).unwrap();
        state.encode_pointer(keys_offset);
        // Encode keys, setup vals
        for (key, value) in self.into_iter() {
            key.encode(encoder, &mut keys_state, keys_context.clone());
            vals_vec.push(value);
        }
        // Encode vals
        vals_vec.encode(encoder, state, context.clone())
    }
    fn decode_value(
        decoder: &mut Decoder,
        context: Context,
    ) -> Result<HashMap<K, V>, ValidationError> {
        let (keys_offset, vals_offset) = {
            let state = decoder.get_mut(&context);
            match state.decode_struct_header(&MAP_VERSIONS) {
                Ok(_) => (),
                Err(err) => return Err(err),
            };
            // Decode the keys pointer and check for overflow
            let keys_offset = match state.decode_pointer() {
                Some(ptr) => ptr,
                None => return Err(ValidationError::IllegalPointer),
            };
            // Decode the keys pointer and check for overflow
            let vals_offset = match state.decode_pointer() {
                Some(ptr) => ptr,
                None => return Err(ValidationError::IllegalPointer),
            };
            if keys_offset == MOJOM_NULL_POINTER || vals_offset == MOJOM_NULL_POINTER {
                return Err(ValidationError::UnexpectedNullPointer);
            }
            (keys_offset as usize, vals_offset as usize)
        };
        let (keys_context, keys_elems) =
            match array_claim_and_decode_header::<K>(decoder, keys_offset) {
                Ok((context, elems)) => (context, elems),
                Err(err) => return Err(err),
            };
        let mut keys_vec: Vec<K> = Vec::with_capacity(keys_elems as usize);
        for _ in 0..keys_elems {
            let key = match K::decode(decoder, keys_context.clone()) {
                Ok(value) => value,
                Err(err) => return Err(err),
            };
            keys_vec.push(key);
        }
        let (vals_context, vals_elems) =
            match array_claim_and_decode_header::<V>(decoder, vals_offset) {
                Ok((context, elems)) => (context, elems),
                Err(err) => return Err(err),
            };
        if keys_elems != vals_elems {
            return Err(ValidationError::DifferentSizedArraysInMap);
        }
        let mut map = HashMap::with_capacity(keys_elems as usize);
        for key in keys_vec.into_iter() {
            let val = match V::decode(decoder, vals_context.clone()) {
                Ok(value) => value,
                Err(err) => return Err(err),
            };
            map.insert(key, val);
        }
        Ok(map)
    }
}

impl<K: MojomEncodable + Eq + Hash, V: MojomEncodable> MojomEncodable for HashMap<K, V> {
    impl_encodable_for_pointer!();
    fn compute_size(&self, context: Context) -> usize {
        let mut size = encoding::align_default(self.serialized_size(&context));
        // The size of the one array
        size += DATA_HEADER_SIZE;
        size += (K::embed_size(&context) * self.len()).as_bytes();
        size = encoding::align_default(size);
        // Any extra space used by the keys
        for (key, _) in self {
            size += key.compute_size(context.clone());
        }
        // Need to re-align after this for the next array
        size = encoding::align_default(size);
        // The size of the one array
        size += DATA_HEADER_SIZE;
        size += (V::embed_size(&context) * self.len()).as_bytes();
        size = encoding::align_default(size);
        // Any extra space used by the values
        for (_, value) in self {
            size += value.compute_size(context.clone());
        }
        // Align one more time at the end to keep the next object aligned.
        encoding::align_default(size)
    }
}

impl<T: MojomEncodable + CastHandle + Handle> MojomHandle for T {}

macro_rules! impl_encodable_for_handle {
    ($handle_type:path) => {
        fn mojom_alignment() -> usize {
            4
        }
        fn mojom_type() -> MojomType {
            MojomType::Handle
        }
        fn embed_size(_context: &Context) -> Bits {
            Bits(8 * mem::size_of::<u32>())
        }
        fn compute_size(&self, _context: Context) -> usize {
            0
        }
        fn encode(self, encoder: &mut Encoder, state: &mut EncodingState, _context: Context) {
            let pos = encoder.add_handle(self.as_untyped());
            state.encode(pos as i32);
        }
        fn decode(
            decoder: &mut Decoder,
            context: Context,
        ) -> Result<$handle_type, ValidationError> {
            let handle_index = {
                let state = decoder.get_mut(&context);
                state.decode::<i32>()
            };
            decoder.claim_handle::<$handle_type>(handle_index)
        }
    };
}

impl MojomEncodable for UntypedHandle {
    impl_encodable_for_handle!(UntypedHandle);
}

impl MojomEncodable for message_pipe::MessageEndpoint {
    impl_encodable_for_handle!(message_pipe::MessageEndpoint);
}

impl MojomEncodable for shared_buffer::SharedBuffer {
    impl_encodable_for_handle!(shared_buffer::SharedBuffer);
}

impl<T> MojomEncodable for data_pipe::Consumer<T> {
    impl_encodable_for_handle!(data_pipe::Consumer<T>);
}

impl<T> MojomEncodable for data_pipe::Producer<T> {
    impl_encodable_for_handle!(data_pipe::Producer<T>);
}
