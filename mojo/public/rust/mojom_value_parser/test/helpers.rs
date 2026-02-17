// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains helper functions for the actual tests.

chromium::import! {
    "//mojo/public/rust/mojom_value_parser:mojom_value_parser_core";
    "//mojo/public/rust/system";
}

use mojom_value_parser_core::MojomValue;
use system::mojo_types::UntypedHandle;

/// This functions allows us to create a dummy handle value for testing
/// purposes. It created and returns a valid but otherwise unspecified handle.
pub(crate) fn dummy_handle() -> UntypedHandle {
    // Create a new message pipe, return one endpoint, and drop the other
    system::message_pipe::MessageEndpoint::create_pipe().unwrap().0.into()
}

/// Compares two `MojomValue`s for equivalence. Equivalence is defined as
/// equality, except all handle values compare equal.
///
/// This function is used to compare values that have handles inside them,
/// because our handle type enforces uniqueness (i.e. no two handle objects
/// are ever equal). By ignoring handles, we can at least verify that the
/// encoding/decoding on the wire are correct.
pub(crate) fn equivalent_value(v1: &MojomValue, v2: &MojomValue) -> bool {
    match (v1, v2) {
        (MojomValue::Invalid, MojomValue::Invalid)
        | (MojomValue::Bool(_), MojomValue::Bool(_))
        | (MojomValue::Int8(_), MojomValue::Int8(_))
        | (MojomValue::UInt8(_), MojomValue::UInt8(_))
        | (MojomValue::Int16(_), MojomValue::Int16(_))
        | (MojomValue::UInt16(_), MojomValue::UInt16(_))
        | (MojomValue::Int32(_), MojomValue::Int32(_))
        | (MojomValue::UInt32(_), MojomValue::UInt32(_))
        | (MojomValue::UInt64(_), MojomValue::UInt64(_))
        | (MojomValue::Float32(_), MojomValue::Float32(_))
        | (MojomValue::Float64(_), MojomValue::Float64(_))
        | (MojomValue::String(_), MojomValue::String(_))
        | (MojomValue::Enum(_), MojomValue::Enum(_)) => v1 == v2,
        (MojomValue::Handle(_), MojomValue::Handle(_)) => true,
        (MojomValue::Nullable(None), MojomValue::Nullable(None)) => true,
        (MojomValue::Union(disc1, boxed1), MojomValue::Union(disc2, boxed2)) => {
            disc1 == disc2 && equivalent_value(boxed1, boxed2)
        }
        (MojomValue::Struct(names1, vals1), MojomValue::Struct(names2, vals2)) => {
            names1 == names2 && equivalent_values(vals1, vals2)
        }
        (MojomValue::Array(vals1), MojomValue::Array(vals2)) => equivalent_values(vals1, vals2),
        (MojomValue::Map(map1), MojomValue::Map(map2)) => {
            // Note that we use BTreeMaps, which guarantee that both these iterators
            // are sorted by key. So it's sufficient to compare them independently.
            equivalent_values(map1.keys(), map2.keys())
                && equivalent_values(map1.values(), map2.values())
        }
        (MojomValue::Nullable(Some(v1)), MojomValue::Nullable(Some(v2))) => {
            equivalent_value(v1, v2)
        }
        _ => false,
    }
}

/// Same as `equivalent_value`, but on iterables
pub(crate) fn equivalent_values<'a>(
    v1s: impl IntoIterator<Item = &'a MojomValue>,
    v2s: impl IntoIterator<Item = &'a MojomValue>,
) -> bool {
    let mut it1 = v1s.into_iter();
    let mut it2 = v2s.into_iter();

    // Sadly, `zip` doesn't let us determine whether the iterators had the
    // same length, and we don't yet have access to `itertools` in chromium, so
    // we have to manually walk down the iterators comparing them element-by-
    // element to make sure they're the same length
    loop {
        match (it1.next(), it2.next()) {
            (Some(v1), Some(v2)) => {
                if !equivalent_value(v1, v2) {
                    return false;
                }
            }
            (None, None) => return true,
            _ => return false,
        }
    }
}
