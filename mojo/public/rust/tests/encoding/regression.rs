// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests validation functionality in the bindings package
//!
//! Test failure is defined as the function returning via panicking
//! and the result being caught in the test! macro. If a test function
//! returns without panicking, it is assumed to pass.

use mojo::bindings::decoding::{Decoder, ValidationError};
use mojo::bindings::encoding::{self, Context, DataHeaderValue, Encoder, EncodingState};
use mojo::bindings::impl_encodable_for_pointer;
use mojo::bindings::mojom::{MojomEncodable, MojomPointer, MojomStruct};
use mojo::system::{self, UntypedHandle};
use rust_gtest_interop::prelude::*;

const STRUCT_A_VERSIONS: [(u32, u32); 1] = [(0, 16)];

struct StructA<T: MojomEncodable> {
    param0: [T; 3],
}

impl<T: MojomEncodable> MojomPointer for StructA<T> {
    fn header_data(&self) -> DataHeaderValue {
        DataHeaderValue::Version(0)
    }
    fn serialized_size(&self, _context: &Context) -> usize {
        16
    }
    fn encode_value(self, encoder: &mut Encoder, state: &mut EncodingState, context: Context) {
        MojomEncodable::encode(self.param0, encoder, state, context.clone());
    }
    fn decode_value(decoder: &mut Decoder, context: Context) -> Result<Self, ValidationError> {
        let _version = {
            let state = decoder.get_mut(&context);
            match state.decode_struct_header(&STRUCT_A_VERSIONS) {
                Ok(header) => header.data(),
                Err(err) => return Err(err),
            }
        };
        let param0 = match <[T; 3]>::decode(decoder, context.clone()) {
            Ok(value) => value,
            Err(err) => return Err(err),
        };
        Ok(StructA { param0: param0 })
    }
}

impl<T: MojomEncodable> MojomEncodable for StructA<T> {
    impl_encodable_for_pointer!();
    fn compute_size(&self, context: Context) -> usize {
        encoding::align_default(self.serialized_size(&context))
            + self.param0.compute_size(context.clone())
    }
}

impl<T: MojomEncodable> MojomStruct for StructA<T> {}

// Fixed size arrays have complex and unsafe semantics to ensure
// there are no memory leaks. We test this behavior here to make
// sure memory isn't becoming corrupted.
stubbed_mojo_test!(MojoRegressionTest, RegressionFixedSizeArrayErrorPropagatesSafely, {
    let handle1 = unsafe { system::acquire(0) };
    let handle2 = unsafe { system::acquire(0) };
    let handle3 = unsafe { system::acquire(0) };
    let val = StructA { param0: [handle1, handle2, handle3] };
    let (mut buffer, mut handles) = val.auto_serialize();
    handles.truncate(1);
    let new_val = <StructA<UntypedHandle>>::deserialize(&mut buffer[..], handles);
    match new_val {
        Ok(_) => expect_true!(false, "Value should not be okay!"),
        Err(err) => expect_eq!(err, ValidationError::IllegalHandle),
    }
});

// Same as the above test, but verifies that drop() is called.
// For the only handle that should drop, we make the handle some
// random number which is potentially a valid handle. When on
// drop() we try to close it, we should panic.
stubbed_mojo_test!(MojoRegressionTest, RegressionFixedSizeArrayVerifyDrop, {
    let handle1 = unsafe { system::acquire(42) };
    let handle2 = unsafe { system::acquire(0) };
    let handle3 = unsafe { system::acquire(0) };
    let val = StructA { param0: [handle1, handle2, handle3] };
    let (mut buffer, mut handles) = val.auto_serialize();
    handles.truncate(1);
    let new_val = <StructA<UntypedHandle>>::deserialize(&mut buffer[..], handles);
    match new_val {
        Ok(_) => expect_true!(false, "Value should not be okay!"),
        Err(err) => expect_eq!(err, ValidationError::IllegalHandle),
    }
});
