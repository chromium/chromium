// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    // `pub` since a macro refers to `$crate::system`.
    pub "//mojo/public/rust:mojo_system" as system;
}

pub mod data;
pub mod mojom;

use std::mem::size_of;

use bytemuck::{Pod, Zeroable};

/// Describes why a mojo message is invalid.
///
/// The `Debug` impl outputs extra helpful metadata.
#[derive(Debug)]
pub struct ValidationError {
    kind: ValidationErrorKind,
    // Include code location info to help see why an error occurred. This is
    // more intended for debugging the implementation itself, rather than for
    // determining why an invalid message is invalid.
    #[allow(dead_code)]
    location: &'static std::panic::Location<'static>,
}

impl ValidationError {
    #[track_caller]
    pub fn new(kind: ValidationErrorKind) -> ValidationError {
        ValidationError { kind, location: std::panic::Location::caller() }
    }

    pub fn kind(&self) -> ValidationErrorKind {
        self.kind
    }
}

pub type Result<T> = std::result::Result<T, ValidationError>;

/// Mojo validation error category.
///
/// This mirrors //mojo/public/cpp/bindings/lib/validation_errors.h; refer to
/// that file for documentation. Do not assume the integer representations are
/// the same as in C++. `VALIDATION_ERROR_NONE` has no corresponding variant;
/// use `Result<(), ValidationError>` if needed.
#[derive(Clone, Copy, Debug)]
pub enum ValidationErrorKind {
    MisalignedObject,
    IllegalMemoryRange,
    UnexpectedStructHeader,
    IllegalPointer,
    UnexpectedNullPointer,
    MessageHeaderInvalidFlags,
    MessageHeaderMissingRequestId,
    MessageHeaderUnknownMethod,
    UnknownEnumValue,
}

impl ValidationErrorKind {
    pub fn to_str(self) -> &'static str {
        use ValidationErrorKind::*;
        match self {
            MisalignedObject => "VALIDATION_ERROR_MISALIGNED_OBJECT",
            IllegalMemoryRange => "VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE",
            UnexpectedStructHeader => "VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER",
            IllegalPointer => "VALIDATION_ERROR_ILLEGAL_POINTER",
            UnexpectedNullPointer => "VALIDATION_ERROR_UNEXPECTED_NULL_POINTER",
            MessageHeaderInvalidFlags => "VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS",
            MessageHeaderMissingRequestId => "VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID",
            MessageHeaderUnknownMethod => "VALIDATION_ERROR_MESSAGE_HEADER_UNKNOWN_METHOD",
            UnknownEnumValue => "VALIDATION_ERROR_UNKNOWN_ENUM_VALUE",
        }
    }
}

/// A read-only view to a serialized mojo message.
pub struct MessageView<'b> {
    // The entire message buffer, including the header.
    buf: &'b [u8],
    // Parsed message header.
    header: data::MessageHeader,
}

impl<'b> MessageView<'b> {
    /// Create from a buffer that contains a serialized message. Validates the
    /// message header.
    pub fn new(buf: &'b [u8]) -> Result<Self> {
        let mut this = MessageView { buf, header: Zeroable::zeroed() };

        let header_chunk = this.get_struct_chunk(AbsolutePointer::<data::MessageHeader> {
            offset: 0,
            _phantom: std::marker::PhantomData,
        })?;
        this.header = header_chunk.read();

        let this = this;

        match this.header.header.version {
            0 => {
                if this.header.header.size as usize != data::MESSAGE_HEADER_V0_SIZE {
                    return Err(ValidationError::new(ValidationErrorKind::UnexpectedStructHeader));
                }

                // Either of these flags require the request_id field, which
                // implies the version should be at least 1.
                if this.header.flags.intersects(
                    data::MessageHeaderFlags::EXPECTS_RESPONSE
                        | data::MessageHeaderFlags::IS_RESPONSE,
                ) {
                    return Err(ValidationError::new(
                        ValidationErrorKind::MessageHeaderMissingRequestId,
                    ));
                }
            }
            1 => {
                if this.header.header.size as usize != data::MESSAGE_HEADER_V1_SIZE {
                    return Err(ValidationError::new(ValidationErrorKind::UnexpectedStructHeader));
                }
            }
            2 => {
                if this.header.header.size as usize != data::MESSAGE_HEADER_V2_SIZE {
                    return Err(ValidationError::new(ValidationErrorKind::UnexpectedStructHeader));
                }
            }
            _ => {
                if (this.header.header.size as usize) < data::MESSAGE_HEADER_V2_SIZE {
                    return Err(ValidationError::new(ValidationErrorKind::UnexpectedStructHeader));
                }
            }
        }

        // These flags are mutually exclusive.
        if this.header.flags.contains(
            data::MessageHeaderFlags::EXPECTS_RESPONSE | data::MessageHeaderFlags::IS_RESPONSE,
        ) {
            return Err(ValidationError::new(ValidationErrorKind::MessageHeaderInvalidFlags));
        }

        Ok(this)
    }

    /// Begin to read a mojom struct at buffer location `ptr`.
    ///
    /// Parses the struct header at `ptr` and returns a view of that struct's
    /// extent. Does not perform any validation of the struct's data.
    pub fn get_struct_chunk<T>(
        &'_ self,
        ptr: AbsolutePointer<T>,
    ) -> Result<MessageViewChunk<'_, 'b, T>> {
        let offset = ptr.offset as usize;

        // Ensure alignment requirement is met.
        if offset % data::OBJECT_ALIGNMENT != 0 {
            return Err(ValidationError::new(ValidationErrorKind::MisalignedObject));
        }

        // Read header to know how many bytes to take.
        let struct_header: data::StructHeader = bytemuck::pod_read_unaligned(
            self.buf
                .get(offset..offset + size_of::<data::StructHeader>())
                .ok_or_else(|| ValidationError::new(ValidationErrorKind::IllegalMemoryRange))?,
        );

        let Some(end) = offset.checked_add(struct_header.size as usize) else {
            return Err(ValidationError::new(ValidationErrorKind::IllegalMemoryRange));
        };

        Ok(MessageViewChunk {
            message: self,
            data: self
                .buf
                .get(offset..end)
                .ok_or_else(|| ValidationError::new(ValidationErrorKind::IllegalMemoryRange))?,
            _phantom: std::marker::PhantomData::<T>,
        })
    }

    /// Byte offset of the payload (method call or response struct) in the
    /// message buffer.
    pub fn payload_offset(&self) -> u64 {
        self.header.header.size as u64
    }

    /// Validate a method call request for mojom interface `I`.
    pub fn validate_request<I: mojom::Interface>(&self) -> Result<()> {
        self.validate::<I>(false)
    }

    /// Validate a method call response for mojom interface `I`.
    pub fn validate_response<I: mojom::Interface>(&self) -> Result<()> {
        self.validate::<I>(true)
    }

    fn validate<I: mojom::Interface>(&self, is_response: bool) -> Result<()> {
        let Some(method_info) = I::get_method_info(self.header.name) else {
            return Err(ValidationError::new(ValidationErrorKind::MessageHeaderUnknownMethod));
        };

        self.validate_impl(method_info, is_response)
    }

    fn validate_impl(&self, method_info: &mojom::MethodInfo, is_response: bool) -> Result<()> {
        let mut ctx =
            ValidationContext { message: self, valid_offset: self.header.header.size as u64 };

        // Ensure the message is the kind we are expected.
        if is_response != self.header.flags.contains(data::MessageHeaderFlags::IS_RESPONSE) {
            return Err(ValidationError::new(ValidationErrorKind::MessageHeaderInvalidFlags));
        }

        // If the request expects a response, ensure the interface bindings
        // agree.
        if !is_response
            && (method_info.validate_response.is_some()
                != self.header.flags.contains(data::MessageHeaderFlags::EXPECTS_RESPONSE))
        {
            return Err(ValidationError::new(ValidationErrorKind::MessageHeaderInvalidFlags));
        }

        // Validate the request or response.
        if is_response {
            if let Some(validate) = method_info.validate_response {
                validate(&mut ctx)
            } else {
                Err(ValidationError::new(ValidationErrorKind::MessageHeaderUnknownMethod))
            }
        } else {
            (method_info.validate_request)(&mut ctx)
        }
    }
}

/// A pointer to a mojom object in a message, represented as an offset from the
/// beginning of the message.
///
/// This is the "absolute" version of `data::Pointer` in that it is absolute for
/// a given mojom message. It is still defined relative to the message.
#[derive(Clone, Copy, Debug)]
pub struct AbsolutePointer<T: ?Sized> {
    pub offset: u64,
    _phantom: std::marker::PhantomData<*mut T>,
}

impl<T: ?Sized> AbsolutePointer<T> {
    /// Given the mojom pointer `rel`, the owning object `owner`, and the
    /// object's `chunk`, calculate the absolute pointer.
    ///
    /// This is safe, but the result will be incorrect or may panic if `rel`
    /// doesn't belong to `owner` and `chunk` is not `owner`'s chunk. Using an
    /// erroneous value is also incorrect but safe.
    pub fn resolve_from<U>(
        rel: &data::Pointer<T>,
        owner: &U,
        chunk: &MessageViewChunk<'_, '_, U>,
    ) -> Result<Self> {
        if rel.offset > u32::MAX as _ {
            return Err(ValidationError::new(ValidationErrorKind::IllegalPointer));
        }

        let ptr_offset_in_object =
            rel as *const _ as usize - owner as *const _ as *const () as usize;
        Ok(AbsolutePointer {
            offset: rel.offset + ptr_offset_in_object as u64 + chunk.offset() as u64,
            _phantom: std::marker::PhantomData,
        })
    }
}

/// A piece of a message containing one object, i.e. a struct value, nested
/// union, array, or map.
pub struct MessageViewChunk<'a, 'b: 'a, T> {
    message: &'a MessageView<'b>,
    data: &'a [u8],
    _phantom: std::marker::PhantomData<T>,
}

impl<'a, 'b: 'a, T> MessageViewChunk<'a, 'b, T> {
    /// Absolute byte offset of this chunk in the parent message.
    pub fn offset(&self) -> usize {
        self.data.as_ptr() as usize - self.message.buf.as_ptr() as usize
    }

    /// Length of this chunk in bytes.
    pub fn len(&self) -> usize {
        self.data.len()
    }
}

impl<'a, 'b: 'a, T: Pod> MessageViewChunk<'a, 'b, T> {
    /// Read a `T`, zeroing fields not present (i.e. if the message's value of
    /// `T` is an older mojom version than ours).
    pub fn read(&self) -> T {
        let mut result = Zeroable::zeroed();

        let bytes_to_copy = std::cmp::min(std::mem::size_of_val(&result), self.data.len());
        bytemuck::bytes_of_mut(&mut result)[0..bytes_to_copy]
            .copy_from_slice(&self.data[0..bytes_to_copy]);

        result
    }
}

/// Common state used when validating a message.
pub struct ValidationContext<'a, 'b: 'a> {
    // The message being validated.
    message: &'a MessageView<'b>,
    // The first valid offset into the message buffer. Any preceding offsets are
    // not valid since they were already claimed by another value, or they
    // violate the depth-first layout of a mojo message.
    valid_offset: u64,
}

impl<'a, 'b: 'a> ValidationContext<'a, 'b> {
    /// The message being validated.
    pub fn message(&self) -> &'a MessageView<'b> {
        self.message
    }

    /// Claim ownership of a mojom struct at buffer location `ptr`.
    ///
    /// The bytes claimed are no longer available for other claims. Since mojom
    /// messages are serialized depth-first, only following bytes can be claimed
    /// later.
    pub fn claim_struct<T>(
        &mut self,
        ptr: AbsolutePointer<T>,
    ) -> Result<MessageViewChunk<'a, 'b, T>> {
        if ptr.offset < self.valid_offset {
            return Err(ValidationError::new(ValidationErrorKind::IllegalMemoryRange));
        }

        let chunk = self.message.get_struct_chunk(ptr)?;
        self.valid_offset = (chunk.offset() + chunk.len()) as u64;
        Ok(chunk)
    }

    /// Claim the root struct of the message payload.
    pub fn claim_payload_root<T>(&mut self) -> Result<MessageViewChunk<'a, 'b, T>> {
        let ptr = AbsolutePointer {
            offset: self.message.payload_offset(),
            _phantom: std::marker::PhantomData,
        };
        self.claim_struct(ptr)
    }
}
