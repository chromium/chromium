// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the `MojomParse` trait, which allows a Rust type to be
//! converted to/from the the Mojom binary format.
//!
//! It operates by providing the facilities to convert `Self` to and from a
//! `MojomValue`, which is the type that's actually used by the parser. The
//! trait has implementations for many builtin types; however, for structs it
//! must be derived using a proc-macro defined in `parsing_attribute.rs`.
//!
//! This file also defines the `PrimitiveEnum` trait, which provides a way to
//! implement `MojomParse` for enums whose variants don't carry any additional
//! data (a C++-style enum). That trait is also derived via macro, and the
//! derivation automatically provides `MojomParse` as well.

chromium::import! {
    "//mojo/public/rust/system";
}

use crate::ast::*;
use crate::pack::pack_mojom_type;
use itertools::Itertools;
use ordered_float::OrderedFloat;
use std::collections::HashMap;

use system::message_pipe::MessageEndpoint;
use system::mojo_types::UntypedHandle;

/// This trait allows a type to be serialized/deserialized into a Mojom message.
///
/// The `Context` parameter allows the trait to specify additional information
/// that is needed for converting to/from `MojomValue`s. In practice, this
/// context value is unused for all types except associated remotes and
/// receivers.
pub trait MojomParse<Context = ()>: Sized + 'static {
    /// Returns the MojomType associated with this rust struct. This function
    /// should always return the same value.
    fn mojom_type() -> MojomType;

    fn into_mojom_value(self, context: &Context) -> MojomValue;

    fn try_from_mojom_value(value: MojomValue, context: &Context) -> anyhow::Result<Self>;

    /// Returns the packed format for this type.
    ///
    /// Logically, this just returns pack_mojom_type(T::mojom_type(), 0).
    /// However, since we call this for every parse/deparse call, it caches the
    /// results of every previous call.
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
                wire_type_ref
            }
            None => {
                // No current entry, initialize it by packing the input type.
                let wire_type_box = Box::new(pack_mojom_type(&Self::mojom_type()));
                let wire_type_ref: &'static MojomWireType = Box::leak(wire_type_box);
                WIRE_TYPE.write().unwrap().insert(TypeId::of::<Self>(), wire_type_ref);
                wire_type_ref
            }
        }
    }
}

// We could _almost_ replace this with the `num_enum` crate, but it doesn't have
// quite the API we want (we want `from_primitive` to return an option but
// respect default values if they exist)
pub trait PrimitiveEnum: Into<i32> + TryFrom<i32, Error = anyhow::Error> + Sized {
    fn is_valid(value: i32) -> bool {
        Self::try_from(value).is_ok()
    }
}

/***************************** */
// Implementations of the traits for various types.
// Note that for some types we derive instead, see parsing_attribute.rs.
/***************************** */

/// Implements the MojomParse trait for a leaf type. (Ab)uses the fact that
/// MojomType and MojomValue use identically-named variants.
macro_rules! mojomparse_leaf_impl {
    ($target_type:ty, $variant:ident) => {
        impl<Context> MojomParse<Context> for $target_type {
            fn mojom_type() -> MojomType {
                MojomType::$variant
            }

            fn into_mojom_value(self, _context: &Context) -> MojomValue {
                MojomValue::$variant(self.into())
            }

            fn try_from_mojom_value(value: MojomValue, _context: &Context) -> anyhow::Result<Self> {
                if let MojomValue::$variant(v) = value {
                    return Ok(v.into());
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

mojomparse_leaf_impl!(u8, UInt8);
mojomparse_leaf_impl!(u16, UInt16);
mojomparse_leaf_impl!(u32, UInt32);
mojomparse_leaf_impl!(u64, UInt64);
mojomparse_leaf_impl!(i8, Int8);
mojomparse_leaf_impl!(i16, Int16);
mojomparse_leaf_impl!(i32, Int32);
mojomparse_leaf_impl!(i64, Int64);
// Floats can correspond either to themselves or their ordered version.
// The latter is only generated by the bindings when used as keys in a map.
mojomparse_leaf_impl!(f32, Float32);
mojomparse_leaf_impl!(f64, Float64);
mojomparse_leaf_impl!(OrderedFloat<f32>, Float32);
mojomparse_leaf_impl!(OrderedFloat<f64>, Float64);

mojomparse_leaf_impl!(bool, Bool);
mojomparse_leaf_impl!(String, String);

mojomparse_leaf_impl!(UntypedHandle, Handle);
mojomparse_leaf_impl!(MessageEndpoint, Handle);

// Implement MojomParse for any type that implements PrimitiveEnum and the other
// requirements for MojomParse.
impl<Context, T> MojomParse<Context> for T
where
    T: PrimitiveEnum + 'static,
{
    fn mojom_type() -> MojomType {
        MojomType::Enum { is_valid: Predicate::new::<T>(&(Self::is_valid as fn(i32) -> bool)) }
    }

    fn into_mojom_value(self, _context: &Context) -> MojomValue {
        MojomValue::Enum(self.into())
    }

    fn try_from_mojom_value(value: MojomValue, _context: &Context) -> anyhow::Result<Self> {
        if let MojomValue::Enum(v) = value {
            Ok(Self::try_from(v)?)
        } else {
            ::anyhow::bail!(
                "Cannot construct a value of type {} from non-enum MojomValue {:?}",
                std::any::type_name::<Self>(),
                value
            )
        }
    }
}
// Implement MojomParse for arrays and vectors
// It would be neat to do this more generally, e.g. anything that can be cast
// to a slice, but rust doesn't have a way for us to prove that the different
// implementations of the trait are disjoint.

impl<Context, T> MojomParse<Context> for Vec<T>
where
    T: MojomParse<Context>,
{
    fn mojom_type() -> MojomType {
        MojomType::Array { element_type: Box::new(T::mojom_type()), num_elements: None }
    }

    fn into_mojom_value(self, context: &Context) -> MojomValue {
        MojomValue::Array(self.into_iter().map(|v| v.into_mojom_value(context)).collect())
    }

    fn try_from_mojom_value(value: MojomValue, context: &Context) -> anyhow::Result<Self> {
        if let MojomValue::Array(v) = value {
            v.into_iter()
                .map(|val| T::try_from_mojom_value(val, context))
                .collect::<anyhow::Result<_>>()
        } else {
            anyhow::bail!(
                "Cannot construct a value of type {} from this MojomValue: {:?}",
                std::any::type_name::<Self>(),
                value
            );
        }
    }
}

impl<Context, T, const N: usize> MojomParse<Context> for [T; N]
where
    T: MojomParse<Context>,
{
    fn mojom_type() -> MojomType {
        MojomType::Array { element_type: Box::new(T::mojom_type()), num_elements: Some(N) }
    }

    fn into_mojom_value(self, context: &Context) -> MojomValue {
        MojomValue::Array(self.into_iter().map(|v| v.into_mojom_value(context)).collect())
    }

    fn try_from_mojom_value(value: MojomValue, context: &Context) -> anyhow::Result<Self> {
        let MojomValue::Array(v) = value else {
            anyhow::bail!(
                "Cannot construct a value of type {} from this MojomValue: {:?}",
                std::any::type_name::<Self>(),
                value
            );
        };

        if v.len() != N {
            anyhow::bail!(
                "Wrong number of values to construct {} from this MojomValue: {:?}",
                std::any::type_name::<Self>(),
                MojomValue::Array(v)
            )
        };

        let arr_of_t: [T; N] = v
            .into_iter()
            .map(|val| T::try_from_mojom_value(val, context))
            .process_results(|iter| {
                // Unwrap will succeed because we just checked the length above.
                iter.collect_array().unwrap()
            })?;
        Ok(arr_of_t)
    }
}

// Implement MojomParse for Maps
// Currently this is only implemented for HashMap, since that's
// probably what most users will want, but we can extend it to
// other map types if we need to.

impl<Context, K, V> MojomParse<Context> for HashMap<K, V>
where
    K: MojomParse<Context> + Eq + std::hash::Hash,
    V: MojomParse<Context>,
{
    fn mojom_type() -> MojomType {
        MojomType::Map {
            key_type: Box::new(K::mojom_type()),
            value_type: Box::new(V::mojom_type()),
        }
    }

    fn into_mojom_value(self, context: &Context) -> MojomValue {
        let hashmap = self
            .into_iter()
            .map(|(k, v)| (k.into_mojom_value(context), v.into_mojom_value(context)))
            .collect();
        MojomValue::Map(hashmap)
    }

    fn try_from_mojom_value(value: MojomValue, context: &Context) -> anyhow::Result<Self> {
        if let MojomValue::Map(hashmap) = value {
            let converted_map: Self = hashmap
                .into_iter()
                .map(|(k, v)| -> anyhow::Result<(K, V)> {
                    Ok((K::try_from_mojom_value(k, context)?, V::try_from_mojom_value(v, context)?))
                })
                // Fail if any of the conversions failed
                .collect::<Result<_, _>>()?;
            Ok(converted_map)
        } else {
            anyhow::bail!(
                "Cannot construct a value of type {} from this MojomValue: {:?}",
                std::any::type_name::<Self>(),
                value
            );
        }
    }
}

// Implement MojomParse for Options

impl<Context, T> MojomParse<Context> for Option<T>
where
    T: MojomParse<Context>,
{
    fn mojom_type() -> MojomType {
        MojomType::Nullable { inner_type: Box::new(T::mojom_type()) }
    }

    fn into_mojom_value(self, context: &Context) -> MojomValue {
        MojomValue::Nullable(self.map(|v| Box::new(v.into_mojom_value(context))))
    }

    fn try_from_mojom_value(value: MojomValue, context: &Context) -> anyhow::Result<Self> {
        if let MojomValue::Nullable(opt) = value {
            opt.map(|v| T::try_from_mojom_value(*v, context)).transpose()
        } else {
            anyhow::bail!(
                "Cannot construct a value of type {} from this MojomValue: {:?}",
                std::any::type_name::<Self>(),
                value
            );
        }
    }
}
