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
        const MOJOM_TYPE: $crate::mojom::MojomType = $crate::mojom::MojomType::Pointer;
        fn embed_size(_context: &$crate::encoding::Context) -> $crate::encoding::Bits {
            $crate::mojom::POINTER_BIT_SIZE
        }
        fn encode(
            self,
            encoder: &mut $crate::encoding::Encoder,
            state: &mut $crate::encoding::EncodingState,
            context: $crate::encoding::Context,
        ) {
            state.encode_pointer(self.encode_new(encoder, context));
        }
        fn decode(
            decoder: &mut $crate::decoding::Decoder,
            context: $crate::encoding::Context,
        ) -> Result<Self, ValidationError> {
            let state = decoder.get_mut(&context);
            let ptr = state.decode_pointer()?;
            if ptr == $crate::mojom::MOJOM_NULL_POINTER {
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
        const MOJOM_TYPE: $crate::mojom::MojomType = $crate::mojom::MojomType::Union;
        fn embed_size(context: &$crate::encoding::Context) -> $crate::encoding::Bits {
            if context.is_union() {
                $crate::mojom::UNION_NESTED_EMBED_SIZE
            } else {
                $crate::mojom::UNION_INLINE_EMBED_SIZE
            }
        }
        fn encode(
            self,
            encoder: &mut $crate::encoding::Encoder,
            state: &mut $crate::encoding::EncodingState,
            context: $crate::encoding::Context,
        ) {
            if context.is_union() {
                $crate::mojom::encode_union_nested(self, encoder, state, context);
            } else {
                $crate::mojom::encode_union_inline(
                    self,
                    encoder,
                    state,
                    context.set_is_union(true),
                );
            }
        }
        fn decode(
            decoder: &mut $crate::decoding::Decoder,
            context: $crate::encoding::Context,
        ) -> Result<Self, ValidationError> {
            if context.is_union() {
                $crate::mojom::decode_union_nested(decoder, context)
            } else {
                $crate::mojom::decode_union_inline(decoder, context.set_is_union(true))
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
        const MOJOM_TYPE: $crate::mojom::MojomType = $crate::mojom::MojomType::Interface;
        fn embed_size(_context: &$crate::encoding::Context) -> $crate::encoding::Bits {
            use std::mem;
            $crate::encoding::Bits(2 * 8 * mem::size_of::<u32>())
        }
        fn compute_size(&self, _context: $crate::encoding::Context) -> usize {
            0 // Indicates that this type is inlined and it adds nothing external to the size
        }
        fn encode(
            self,
            encoder: &mut $crate::encoding::Encoder,
            state: &mut $crate::encoding::EncodingState,
            context: $crate::encoding::Context,
        ) {
            let version = self.version();
            let pos = encoder.add_handle(self.as_untyped());
            state.encode(pos as i32);
            state.encode(version as u32);
        }
        fn decode(
            decoder: &mut $crate::decoding::Decoder,
            context: $crate::encoding::Context,
        ) -> Result<Self, ValidationError> {
            let state = decoder.get_mut(&context);
            let handle_index: i32 = state.decode();
            let version: u32 = state.decode();
            let handle = decoder
                .claim_handle::<$crate::system::message_pipe::MessageEndpoint>(handle_index)?;
            Ok(Self::with_version(handle, version))
        }
    };
}
