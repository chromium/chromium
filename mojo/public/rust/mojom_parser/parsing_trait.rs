// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ast::*;

/// This trait allows a type to be serialized/deserialized into a Mojom message.
pub trait MojomParse: Sized {
    /// Returns the MojomType associated with this rust struct. This function
    /// should always return the same value.
    fn mojom_type() -> MojomType;

    // FOR_RELEASE: We could use serde to generate these automatically for us, but I don't
    // think we could generate the mojom_type function so not sure it's worth it vs. just
    // writing it all together manually.
    // FOR_RELEASE: See if we benefit from taking by value instead of/in addition to by ref
    fn to_mojom_value(&self) -> MojomValue;
    // FOR_RELEASE: Either panic or return a nice error message
    fn from_mojom_value(v: MojomValue) -> Option<Self>;
}

impl MojomParse for u8 {
    fn mojom_type() -> MojomType {
        MojomType::UInt8
    }

    fn to_mojom_value(&self) -> MojomValue {
        MojomValue::UInt8(*self)
    }

    fn from_mojom_value(v: MojomValue) -> Option<Self> {
        if let MojomValue::UInt8(n) = v {
            return Some(n);
        } else {
            return None;
        }
    }
}

// Continue for all the primitive types...
// And then implement for structured types by calling the appropriate method on
// each component?
