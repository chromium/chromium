// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// This macro provides a common implementation of MojomEncodable
/// for MojomPointer types.
///
/// Note: it does not implement compute_size();
///
/// The Rust type system currently lacks the facilities to do this
/// generically (need mutually excludable traits) but this macro
/// should be replaced as soon as this is possible.
#[macro_export]
macro_rules! impl_encodable_for_pointer {
    () => {
        fn mojom_alignment() -> usize {
            8 // All mojom pointers are 8 bytes in length, and thus are 8-byte aligned
        }
        fn mojom_type() -> $crate::bindings::mojom::MojomType {
            $crate::bindings::mojom::MojomType::Pointer
        }
        fn embed_size(
            _context: &$crate::bindings::encoding::Context,
        ) -> $crate::bindings::encoding::Bits {
            $crate::bindings::mojom::POINTER_BIT_SIZE
        }
        fn encode(
            self,
            encoder: &mut $crate::bindings::encoding::Encoder,
            context: $crate::bindings::encoding::Context,
        ) {
            let loc = encoder.size() as u64;
            {
                let state = encoder.get_mut(&context);
                state.encode_pointer(loc);
            }
            self.encode_new(encoder, context);
        }
        fn decode(
            decoder: &mut $crate::bindings::decoding::Decoder,
            context: $crate::bindings::encoding::Context,
        ) -> Result<Self, ValidationError> {
            let ptr = {
                let state = decoder.get_mut(&context);
                match state.decode_pointer() {
                    Some(ptr) => ptr,
                    None => return Err(ValidationError::IllegalPointer),
                }
            };
            if ptr == $crate::bindings::mojom::MOJOM_NULL_POINTER {
                Err(ValidationError::UnexpectedNullPointer)
            } else {
                Self::decode_new(decoder, context, ptr)
            }
        }
    };
}

/// This macro provides a common implementation of MojomEncodable
/// for MojomUnion types.
///
/// Note: it does not implement compute_size();
///
/// The Rust type system currently lacks the facilities to do this
/// generically (need mutually excludable traits) but this macro
/// should be replaced as soon as this is possible.
#[macro_export]
macro_rules! impl_encodable_for_union {
    () => {
        fn mojom_alignment() -> usize {
            8
        }
        fn mojom_type() -> $crate::bindings::mojom::MojomType {
            $crate::bindings::mojom::MojomType::Union
        }
        fn embed_size(
            context: &$crate::bindings::encoding::Context,
        ) -> $crate::bindings::encoding::Bits {
            if context.is_union() { Self::nested_embed_size() } else { Self::inline_embed_size() }
        }
        fn encode(
            self,
            encoder: &mut $crate::bindings::encoding::Encoder,
            context: $crate::bindings::encoding::Context,
        ) {
            if context.is_union() {
                self.nested_encode(encoder, context);
            } else {
                self.inline_encode(encoder, context.set_is_union(true));
            }
        }
        fn decode(
            decoder: &mut $crate::bindings::decoding::Decoder,
            context: $crate::bindings::encoding::Context,
        ) -> Result<Self, ValidationError> {
            if context.is_union() {
                Self::nested_decode(decoder, context)
            } else {
                Self::inline_decode(decoder, context.set_is_union(true))
            }
        }
    };
}

/// This macro provides a common implementation of MojomEncodable
/// for MojomInterface types.
///
/// Note: it does not implement compute_size();
///
/// The Rust type system currently lacks the facilities to do this
/// generically (need mutually excludable traits) but this macro
/// should be replaced as soon as this is possible.
#[macro_export]
macro_rules! impl_encodable_for_interface {
    () => {
        fn mojom_alignment() -> usize {
            4
        }
        fn mojom_type() -> $crate::bindings::mojom::MojomType {
            $crate::bindings::mojom::MojomType::Interface
        }
        fn embed_size(
            _context: &$crate::bindings::encoding::Context,
        ) -> $crate::bindings::encoding::Bits {
            use std::mem;
            $crate::bindings::encoding::Bits(2 * 8 * mem::size_of::<u32>())
        }
        fn compute_size(&self, _context: $crate::bindings::encoding::Context) -> usize {
            0 // Indicates that this type is inlined and it adds nothing external to the size
        }
        fn encode(
            self,
            encoder: &mut $crate::bindings::encoding::Encoder,
            context: $crate::bindings::encoding::Context,
        ) {
            let version = self.version();
            let pos = encoder.add_handle(self.as_untyped());
            let mut state = encoder.get_mut(&context);
            state.encode(pos as i32);
            state.encode(version as u32);
        }
        fn decode(
            decoder: &mut $crate::bindings::decoding::Decoder,
            context: $crate::bindings::encoding::Context,
        ) -> Result<Self, ValidationError> {
            let (handle_index, version) = {
                let mut state = decoder.get_mut(&context);
                (state.decode::<i32>(), state.decode::<u32>())
            };
            let handle = decoder
                .claim_handle::<$crate::system::message_pipe::MessageEndpoint>(handle_index)?;
            Ok(Self::with_version(handle, version))
        }
    };
}
