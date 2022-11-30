// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_UNION_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_UNION_TRAITS_H_

#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom union. |DataViewType| is the corresponding data view type of the
// mojom union. For example, if the mojom union is example.Foo, |DataViewType|
// will be example::FooDataView, which can also be referred to by
// example::Foo::DataView (in chromium) and example::blink::Foo::DataView (in
// blink).
//
// Similar to StructTraits, each specialization of UnionTraits implements the
// following methods:
//   1. Getters for each field in the Mojom type.
//   2. Read() method.
//   3. [Optional] IsNull() and SetToNull().
// Please see the documentation of StructTraits for details of these methods.
//
// Unlike StructTraits, there is one more method to implement:
//   4. A static GetTag() method indicating which field is the current active
//      field for serialization:
//
//        static DataViewType::Tag GetTag(const T& input);
//
//      During serialization, only the field getter corresponding to this tag
//      will be called.
//
template <typename DataViewType, typename T>
struct UnionTraits {
  static_assert(internal::AlwaysFalse<T>::value,
                "Cannot find the mojo::UnionTraits specialization. Did you "
                "forget to include the corresponding header file?");
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_UNION_TRAITS_H_
