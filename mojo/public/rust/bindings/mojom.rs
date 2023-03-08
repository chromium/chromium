// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::decoding::{Decoder, DecodingState, ValidationError};
use crate::encoding;
use crate::encoding::{
    Bits, Context, DataHeader, DataHeaderValue, Encoder, EncodingState, DATA_HEADER_SIZE,
};
use crate::impl_encodable_for_pointer;
use crate::message::MessageHeader;

use std::cmp::Eq;
use std::collections::HashMap;
use std::hash::Hash;
use std::mem;
use std::vec::Vec;

use system::data_pipe;
use system::message_pipe;
use system::shared_buffer;
use system::{CastHandle, Handle, MojoResult, UntypedHandle};

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
#[derive(Clone, Copy, Debug)]
pub enum MojomType {
    Simple,
    Pointer,
    Union,
    Handle,
    Interface,
}

impl MojomType {
    /// Encodes a null value in `state` if this type is nullable. Panics
    /// otherwise.
    pub fn encode_null(self, state: &mut EncodingState) {
        match self {
            MojomType::Simple => unimplemented!("MojomType {self:?} is not nullable"),
            MojomType::Pointer => state.encode_null_pointer(),
            MojomType::Union => state.encode_null_union(),
            MojomType::Handle => state.encode_null_handle(),
            MojomType::Interface => {
                state.encode_null_handle();
                state.encode(0u32);
            }
        }
    }

    /// If the current value of this type in `state` is null, skips it. Returns
    /// whether the value was skipped. Panics if the type is non-nullable.
    pub fn skip_if_null(self, state: &mut DecodingState) -> bool {
        match self {
            MojomType::Simple => unimplemented!("MojomType {self:?} is not nullable"),
            MojomType::Pointer => state.skip_if_null_pointer(),
            MojomType::Union => state.skip_if_null_union(),
            MojomType::Handle => state.skip_if_null_handle(),
            MojomType::Interface => state.skip_if_null_interface(),
        }
    }
}

/// Whatever implements this trait can be serialized in the Mojom format.
pub trait MojomEncodable: Sized {
    const MOJOM_TYPE: MojomType;

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
        let context = decoder.claim(pointer as usize)?;
        Self::decode_value(decoder, context)
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
    let state = decoder.get_mut(&context);
    let global_offset = state.decode_pointer()?;
    if global_offset == MOJOM_NULL_POINTER {
        return Err(ValidationError::UnexpectedNullPointer);
    }
    let context = decoder.claim(global_offset as usize)?;
    T::decode_value(decoder, context)
}

/// Decode a union stored inline in the current context.
pub fn decode_union_inline<T: MojomUnion>(
    decoder: &mut Decoder,
    context: Context,
) -> Result<T, ValidationError> {
    let state = decoder.get_mut(&context);
    state.align_to_byte();
    state.align_to_bytes(8);
    let value = T::decode_value(decoder, context.clone())?;

    let state = decoder.get_mut(&context);
    state.align_to_byte();
    state.align_to_bytes(8);

    Ok(value)
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
        self.pipe().write(&buffer, handles).into_result().map_err(MojomSendError::FailedWrite)
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
        let (buffer, handles) = self.pipe().read().map_err(MojomRecvError::FailedRead)?;
        let msg = Self::Container::decode_message(buffer, handles)
            .map_err(MojomRecvError::FailedValidation)?;
        Ok(msg)
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
            const MOJOM_TYPE: MojomType = MojomType::Simple;
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
    const MOJOM_TYPE: MojomType = MojomType::Simple;
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
    const MOJOM_TYPE: MojomType = T::MOJOM_TYPE;
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
        // TODO(crbug.com/1274864, crbug.com/657632): support nullability for
        // other types.
        match self {
            Some(value) => value.encode(encoder, state, context),
            None => T::MOJOM_TYPE.encode_null(state),
        }
    }
    fn decode(decoder: &mut Decoder, context: Context) -> Result<Self, ValidationError> {
        // TODO(crbug.com/1274864, crbug.com/657632): support nullability for
        // other types.
        if T::MOJOM_TYPE.skip_if_null(decoder.get_mut(&context)) {
            return Ok(None);
        }
        T::decode(decoder, context).map(|val| Some(val))
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
        let state = decoder.get_mut(&context);
        let elems = state.decode_array_header::<T>()?.data();
        let mut value = Vec::with_capacity(elems as usize);
        for _ in 0..elems {
            value.push(T::decode(decoder, context.clone())?);
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
        let state = decoder.get_mut(&context);
        let len = state.decode_array_header::<T>()?.data();
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
        Ok(Vec::<T>::decode_value(decoder, context)?.into_boxed_slice())
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
        let elems = state.decode_array_header::<u8>()?.data();
        let mut value = Vec::with_capacity(elems as usize);
        for _ in 0..elems {
            value.push(state.decode::<u8>());
        }
        Ok(String::from_utf8(value).expect("invalid utf8 in decoded string"))
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
    let context = decoder.claim(offset)?;
    let state = decoder.get_mut(&context);
    let elems = state.decode_array_header::<T>()?.data();
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

        let mut keys = Vec::with_capacity(elems);
        let mut vals = Vec::with_capacity(elems);
        for (key, val) in self.into_iter() {
            keys.push(key);
            vals.push(val);
        }

        keys.encode(encoder, state, context.clone());
        vals.encode(encoder, state, context.clone());
    }
    fn decode_value(
        decoder: &mut Decoder,
        context: Context,
    ) -> Result<HashMap<K, V>, ValidationError> {
        let state = decoder.get_mut(&context);
        state.decode_struct_header(&MAP_VERSIONS)?;

        let keys_offset = state.decode_pointer()?;
        let vals_offset = state.decode_pointer()?;
        if keys_offset == MOJOM_NULL_POINTER || vals_offset == MOJOM_NULL_POINTER {
            return Err(ValidationError::UnexpectedNullPointer);
        }

        let (keys_context, keys_elems) =
            array_claim_and_decode_header::<K>(decoder, keys_offset as usize)?;
        let mut keys: Vec<K> = Vec::with_capacity(keys_elems as usize);
        for _ in 0..keys_elems {
            keys.push(K::decode(decoder, keys_context.clone())?);
        }

        let (vals_context, vals_elems) =
            array_claim_and_decode_header::<V>(decoder, vals_offset as usize)?;
        if keys_elems != vals_elems {
            return Err(ValidationError::DifferentSizedArraysInMap);
        }
        let mut map = HashMap::with_capacity(keys_elems as usize);
        for key in keys.into_iter() {
            let val = V::decode(decoder, vals_context.clone())?;
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
        const MOJOM_TYPE: MojomType = MojomType::Handle;
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
