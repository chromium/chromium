// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FOR_RELASE: Docs

use crate::ast::*;
use crate::pack::pack_mojom_type;

/// This trait allows a type to be serialized/deserialized into a Mojom message.
pub trait MojomParse:
    Into<MojomValue> + TryFrom<MojomValue, Error = anyhow::Error> + Sized + 'static
{
    /// Returns the MojomType associated with this rust struct. This function
    /// should always return the same value.
    fn mojom_type() -> MojomType;

    /// Returns the packed format for this type.
    ///
    /// Logically, this just returns pack_mojom_type(T::mojom_type(), 0).
    /// However, since we call this for every parse/deparse call, it caches the
    /// results of every previous call.
    ///
    /// FOR_RELEASE: We could reduce our complexity by using the generic_static
    /// crate
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
        }

        impl From<$target_type> for MojomValue {
            fn from(value: $target_type) -> MojomValue {
                MojomValue::$variant(value)
            }
        }

        impl TryFrom<MojomValue> for $target_type {
            type Error = anyhow::Error;

            fn try_from(value: MojomValue) -> anyhow::Result<$target_type> {
                if let MojomValue::$variant(v) = value {
                    return Ok(v);
                } else {
                    anyhow::bail!(
                        "Cannot construct a value of type {} from this MojomValue: {:?}",
                        std::any::type_name::<$target_type>(),
                        value
                    );
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

// FOR_RELEASE: We could replace this with one of a number of crates. num_enum
// seems closest, though it doesn't have quite the API we want (we want
// from_primitive to return an option but respect default values if they exist)
pub trait PrimitiveEnum: Into<u32> + TryFrom<u32, Error = anyhow::Error> + Sized {
    fn is_valid(value: u32) -> bool {
        Self::try_from(value).is_ok()
    }
}

impl<T: PrimitiveEnum> From<T> for MojomValue {
    fn from(value: T) -> MojomValue {
        MojomValue::Enum(value.into())
    }
}

// Logically we could implement TryFrom<MojomValue> for T here, but the
// compiler won't let us since T is uncovered, so we derive it instead.

impl<T: PrimitiveEnum + TryFrom<MojomValue, Error = anyhow::Error> + 'static> MojomParse for T {
    fn mojom_type() -> MojomType {
        MojomType::Enum { is_valid: Predicate::new::<T>(&(Self::is_valid as fn(u32) -> bool)) }
    }
}
