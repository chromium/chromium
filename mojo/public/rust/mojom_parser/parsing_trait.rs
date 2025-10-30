// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FOR_RELASE: Docs

use crate::ast::*;
use crate::pack::pack_mojom_type;

/// This trait allows a type to be serialized/deserialized into a Mojom message.
pub trait MojomParse: Sized + 'static {
    /// Returns the MojomType associated with this rust struct. This function
    /// should always return the same value.
    fn mojom_type() -> MojomType;

    // FOR_RELEASE: We could use serde to generate these automatically for us, but I don't
    // think we could generate the mojom_type function so not sure it's worth it vs. just
    // writing it all together manually.
    fn to_mojom_value(self) -> MojomValue;
    // FOR_RELEASE: Either panic or return a nice error message
    fn from_mojom_value(v: MojomValue) -> Option<Self>;

    /// Returns the packed format for this type.
    ///
    /// Logically, this just returns pack_mojom_type(T::mojom_type(), 0).
    /// However, since we call this for every parse/deparse call, it caches the
    /// results of every previous call.
    ///
    /// FOR_RELEASE: We could reduce our complexity by using generic_static crate
    fn wire_type() -> &'static MojomWireType {
        use std::any::TypeId;
        use std::collections::HashMap;
        use std::sync::{LazyLock, RwLock};

        // The cache maps rust types to their corresponding packed format, using
        // TypeId to represent them at runtime.
        type WireTypeCache = HashMap<TypeId, &'static MojomWireType>;

        // Sadly, we can't initialize an RwLock static directly because the initializer
        // wouldn't be a constant expression, so have to wrap it in LazyLock.
        static WIRE_TYPE: LazyLock<RwLock<WireTypeCache>> =
            LazyLock::new(|| RwLock::new(WireTypeCache::new()));

        // The read can only fail if a writer panicked at some point; packing never
        // panics so we know it's safe to unwrap here.
        // `cloned` transforms Option<&& MojomWireType> -> Option<& MojomWireType>
        let contents: Option<&'static MojomWireType> =
            WIRE_TYPE.read().unwrap().get(&TypeId::of::<Self>()).cloned();

        match contents {
            Some(wire_type_ref) => {
                // Computed this before, return the cached result.
                return wire_type_ref;
            }
            None => {
                // No current entry, initialize it by packing the input type.
                let wire_type_box = Box::new(pack_mojom_type(&Self::mojom_type(), 0));
                let wire_type_ref: &'static MojomWireType = Box::leak(wire_type_box);
                WIRE_TYPE.write().unwrap().insert(TypeId::of::<Self>(), wire_type_ref);
                return wire_type_ref;
            }
        }
    }
}

/// Implements the MojomParse trait for a leaf type. (Ab)uses the fact that
/// MojomType and MojomValue use identically-named variants.
macro_rules! mojom_parse_leaf_impl {
    ($target_type:ty, $variant:ident) => {
        impl MojomParse for $target_type {
            fn mojom_type() -> MojomType {
                MojomType::$variant
            }

            fn to_mojom_value(self) -> MojomValue {
                MojomValue::$variant(self)
            }

            fn from_mojom_value(v: MojomValue) -> Option<Self> {
                if let MojomValue::$variant(n) = v {
                    return Some(n);
                } else {
                    return None;
                }
            }
        }
    };
}

mojom_parse_leaf_impl!(u8, UInt8);
mojom_parse_leaf_impl!(u16, UInt16);
mojom_parse_leaf_impl!(u32, UInt32);
mojom_parse_leaf_impl!(u64, UInt64);
mojom_parse_leaf_impl!(i8, Int8);
mojom_parse_leaf_impl!(i16, Int16);
mojom_parse_leaf_impl!(i32, Int32);
mojom_parse_leaf_impl!(i64, Int64);

mojom_parse_leaf_impl!(bool, Bool);
mojom_parse_leaf_impl!(String, String);
