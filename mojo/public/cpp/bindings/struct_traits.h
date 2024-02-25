// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_

#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom struct. |DataViewType| is the corresponding data view type of the
// mojom struct. For example, if the mojom struct is example.Foo,
// |DataViewType| will be example::FooDataView, which can also be referred to by
// example::Foo::DataView (in chromium) and example::blink::Foo::DataView (in
// blink).
//
// Each specialization needs to implement a few things:
//   1. Static getters for each field in the Mojom type. These should be
//      of the form:
//
//        static <return type> <field name>(const T& input);
//
//      and should return a serializable form of the named field as extracted
//      from |input|.
//
//      Serializable form of a field:
//        Value or reference of the same type used in the generated stuct
//        wrapper type, or the following alternatives:
//        - string:
//          Value or reference of any type that has a StringTraits defined.
//          Supported by default: std::string_view, std::string,
//          WTF::String (in blink).
//
//        - array:
//          Value or reference of any type that has an ArrayTraits defined.
//          Supported by default: std::vector, CArray, WTF::Vector (in blink)
//
//        - map:
//          Value or reference of any type that has a MapTraits defined.
//          Supported by default: std::map, std::unordered_map, base::flat_map,
//          WTF::HashMap (in blink).
//
//        - struct:
//          Value or reference of any type that has a StructTraits defined.
//
//        - enum:
//          Value of any type that has an EnumTraits defined.
//
//      For any nullable string/struct/array/map/union field you could also
//      return value or reference of std::optional<T>, if T has the right
//      *Traits defined.
//
//      During serialization, getters for all fields are called exactly once. It
//      is therefore reasonably efficient for a getter to construct and return
//      temporary value in the event that it cannot return a readily
//      serializable reference to some existing object.
//
//   2. A static Read() method to set the contents of a |T| instance from a
//      DataViewType.
//
//        static bool Read(DataViewType data, T* output);
//
//      The generated DataViewType provides a convenient, inexpensive view of a
//      serialized struct's field data. The caller guarantees that
//      |!data.is_null()|.
//
//      Returning false indicates invalid incoming data and causes the message
//      pipe receiving it to be disconnected. Therefore, you can do custom
//      validation for |T| in this method.
//
//   3. [Optional] A static IsNull() method indicating whether a given |T|
//      instance is null:
//
//        static bool IsNull(const T& input);
//
//      This method is called exactly once during serialization, and if it
//      returns |true|, it is guaranteed that none of the getters (described in
//      section 1) will be called for the same |input|. So you don't have to
//      check whether |input| is null in those getters.
//
//      If it is not defined, |T| instances are always considered non-null.
//
//      [Optional] A static SetToNull() method to set the contents of a given
//      |T| instance to null.
//
//        static void SetToNull(T* output);
//
//      When a null serialized struct is received, the deserialization code
//      calls this method instead of Read().
//
//      NOTE: It is to set |*output|'s contents to a null state, not to set the
//      |output| pointer itself to null. "Null state" means whatever state you
//      think it makes sense to map a null serialized struct to.
//
//      If it is not defined, null is not allowed to be converted to |T|. In
//      that case, an incoming null value is considered invalid and causes the
//      message pipe to be disconnected.
//
// In the description above, methods having an |input| parameter define it as
// const reference of T. Actually, it can be a non-const reference of T too.
// E.g., if T contains Mojo handles or interfaces whose ownership needs to be
// transferred. Correspondingly, it requies you to always give non-const T
// reference/value to the Mojo bindings for serialization:
//    - if T is used in the "type_mappings" section of a typemap config file,
//      you need to declare it as pass-by-value:
//        type_mappings = [ "MojomType=T[move_only]" ]
//      or
//        type_mappings = [ "MojomType=T[copyable_pass_by_value]" ]
//
//    - if another type U's StructTraits/UnionTraits has a getter for T, it
//      needs to return non-const reference/value.
//
// EXAMPLE:
//
// Mojom definition:
//   struct Bar {};
//   struct Foo {
//     int32 f_integer;
//     string f_string;
//     array<string> f_string_array;
//     Bar f_bar;
//   };
//
// StructTraits for Foo:
//   template <>
//   struct StructTraits<FooDataView, CustomFoo> {
//     // Optional methods dealing with null:
//     static bool IsNull(const CustomFoo& input);
//     static void SetToNull(CustomFoo* output);
//
//     // Field getters:
//     static int32_t f_integer(const CustomFoo& input);
//     static const std::string& f_string(const CustomFoo& input);
//     static const std::vector<std::string>& f_string_array(
//         const CustomFoo& input);
//     // Assuming there is a StructTraits<Bar, CustomBar> defined.
//     static const CustomBar& f_bar(const CustomFoo& input);
//
//     static bool Read(FooDataView data, CustomFoo* output);
//   };
//
template <typename DataViewType, typename T>
struct StructTraits {
  static_assert(internal::AlwaysFalse<T>::value,
                "Cannot find the mojo::StructTraits specialization. Did you "
                "forget to include the corresponding header file?");
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_
